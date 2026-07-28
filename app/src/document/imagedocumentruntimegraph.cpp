// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimegraph.h"

#include "archive/mediaentrysourcestore.h"
#include "imagedocumentdeletioncontroller.h"
#include "imagedocumentnavigationcontroller.h"
#include "imagedocumentnavigationruntimeplan.h"
#include "imagedocumentpredecodecontroller.h"
#include "imagedocumentpredecodedimagelookup.h"
#include "imagedocumentruntimedependencies.h"
#include "imagedocumentruntimeworkflow.h"
#include "imagedocumentsourceloadrequest.h"
#include "imagedocumentstate.h"
#include "imageopencontroller.h"
#include "imageviewportintegrationruntime.h"
#include "navigation/imagedocumentpagenavigationservice.h"
#include "navigation/navigationlogging.h"
#include "presentation/imagespreadpresentationcontroller.h"
#include "rendering/displayimagestore.h"
#include "rendering/imageviewportdecodesource.h"

#include <QObject>
#include <QUrl>
#include <QtGlobal>
#include <QtMath>
#include <optional>
#include <utility>
#include <variant>

namespace kiriview {
namespace {
    ImageLoadFailure viewportPresentationFailure(
        const ImageLoadSession& session, QString userMessage, QString diagnosticDetail)
    {
        return {
            session.imageUrl(),
            session.id(),
            ImageLoadFailureKind::Presentation,
            DecodedImageFailureRoute::Unknown,
            DecodedImageFailureOperation::Unknown,
            std::move(userMessage),
            std::move(diagnosticDetail),
            ImageLoadFailureSeverity::Error,
            false,
        };
    }

    void logMediaEntrySourceError(const char* message, const MediaEntrySourceError& error)
    {
        qCWarning(kiriviewNavigationLog).noquote() << message << error;
    }

    std::optional<StaticDisplayImagePayload> authoritativeSeed(
        const std::optional<PredecodedImage>& predecoded)
    {
        if (!predecoded.has_value()) {
            return std::nullopt;
        }

        StaticDisplayImagePayload seed = predecoded->displayImage;
        if (!predecoded->embeddedMetadata.isEmpty()) {
            seed.embeddedMetadata = predecoded->embeddedMetadata;
        }
        return seed;
    }
}

struct ImageDocumentRuntimeGraph::PreparedViewportTargetState
{
    ImageLoadSession selectedSession;
    std::optional<ImageLoadSession> resolvedSession;
    ImageDecodeDependencies dependencies;
    std::optional<PredecodedImage> predecoded;
    std::optional<ImageViewportProvisionalPreviewPolicy> provisionalPreviewPolicyOverride;
    std::function<bool()> hasAuthoritativeDisplay;
    std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>
        findPredecodedImage;
    std::weak_ptr<ImageViewportProviderResource> activeResource;
    std::weak_ptr<ImageViewportDecodeProviderSource> activeSource;

    [[nodiscard]] ImageViewportProvisionalPreviewPolicy provisionalPreviewPolicy() const
    {
        if (provisionalPreviewPolicyOverride.has_value()) {
            return *provisionalPreviewPolicyOverride;
        }
        return hasAuthoritativeDisplay && hasAuthoritativeDisplay()
            ? ImageViewportProvisionalPreviewPolicy::Suppress
            : ImageViewportProvisionalPreviewPolicy::Allow;
    }

    void refreshPredecodedImage()
    {
        if (!resolvedSession.has_value() || !findPredecodedImage) {
            return;
        }
        std::optional<PredecodedImage> candidate = findPredecodedImage(resolvedSession->location());
        if (candidate.has_value() && candidate->location == resolvedSession->location()
            && candidate->displayImage.isAuthoritative()) {
            predecoded = std::move(candidate);
        }
    }
};

ImageDocumentRuntimeGraph::ImageDocumentRuntimeGraph(QObject* documentObject,
    ImageDocumentState& state, ImageDocumentRuntimeDependencyOverrides dependencies,
    ImageDocumentRuntimeGraphCallbacks callbacks)
    : m_callbacks(std::move(callbacks))
    , m_state(state)
{
    if (!m_callbacks.notify || !m_callbacks.loadSource || !m_callbacks.resolveExternalSource) {
        qFatal("Image-document runtime graph requires all command and state callbacks");
    }

    ImageDocumentRuntimeDependencies runtimeDependencies
        = resolveImageDocumentRuntimeDependencies(std::move(dependencies));
    ExternalPredecodedImageFinder externalPredecodedImageFinder
        = std::move(runtimeDependencies.externalPredecodedImageFinder);

    composeSurfaceAndPresentation(runtimeDependencies);
    composeNavigationAndCandidatePorts(runtimeDependencies);
    composeWorkflowOwners(
        documentObject, state, runtimeDependencies, std::move(externalPredecodedImageFinder));
    composeWorkflowDispatch();
}

ImageDocumentRuntimeGraph::~ImageDocumentRuntimeGraph() = default;

void ImageDocumentRuntimeGraph::composeSurfaceAndPresentation(
    ImageDocumentRuntimeDependencies& dependencies)
{
    m_viewportDisplayStore = std::make_shared<DisplayImageStore>(
        dependencies.cacheBudgets.displayImageCacheByteBudget);
    m_viewportIntegration = std::make_unique<ImageViewportIntegrationRuntime>(
        ImageViewportIntegrationRuntime::Callbacks {
            [this](const ImageViewportIntegrationProjection& projection) {
                handleViewportProjection(projection);
            },
        });
}

void ImageDocumentRuntimeGraph::composeNavigationAndCandidatePorts(
    ImageDocumentRuntimeDependencies& dependencies)
{
    m_navigationService = std::make_unique<ImageDocumentPageNavigationService>(
        dependencies.candidateProvider,
        ImageDocumentPageNavigationService::Callbacks {
            [this](const ImageDocumentPageNavigationPlan& plan) {
                dispatchPlan(imageDocumentRuntimePlanForNavigationPlan(
                    plan, pageNavigationOpenedCollectionScope()));
            },
            [this](const ImageDocumentPageNavigationCommit& commit) {
                dispatchTransaction(ImageDocumentRuntimeTransaction {
                    commit.pageNavigationChanged
                        ? std::vector<ImageDocumentChange> { ImageDocumentChange::PageNavigation }
                        : std::vector<ImageDocumentChange> {},
                    imageDocumentRuntimePlanForNavigationPlan(
                        commit.effects, pageNavigationOpenedCollectionScope()),
                });
            },
            [this]() { return m_deletionController->inProgress(); },
            m_callbacks.resolveExternalSource,
        });
}

OpenedCollectionScopeLocation ImageDocumentRuntimeGraph::pageNavigationOpenedCollectionScope() const
{
    const std::optional<ImageDocumentPageCandidateListContext> context
        = m_navigationService->selectedPageCandidateContext();
    if (context.has_value()) {
        return context->openedCollectionScope();
    }

    return m_state.displayedOpenedCollectionScope();
}

void ImageDocumentRuntimeGraph::composeWorkflowOwners(QObject* documentObject,
    ImageDocumentState& state, ImageDocumentRuntimeDependencies& dependencies,
    ExternalPredecodedImageFinder externalPredecodedImageFinder)
{
    m_imageDecodeDependencies = dependencies.imageDecode;
    m_mediaEntrySourceStore = std::move(dependencies.mediaEntrySourceStore);
    m_deletionController = std::make_unique<ImageDocumentDeletionController>(
        documentObject, state,
        [this]() {
            return m_viewportIntegration->projection().correlated
                && !m_viewportIntegration->projection().displayedUrl.isEmpty();
        },
        dependencies.candidateProvider, std::move(dependencies.fileDeletionProvider),
        ImageDocumentDeletionController::Callbacks {
            [this]() {
                m_callbacks.notify(std::vector<ImageDocumentChange> {
                    ImageDocumentChange::FileDeletionInProgress });
            },
            [this](const ImageDocumentRuntimePlan& plan) { dispatchPlan(plan); },
            std::move(m_callbacks.fileDeletionFailed),
        },
        m_callbacks.resolveExternalSource);
    m_predecodeController = std::make_unique<ImageDocumentPredecodeController>(
        state, [this]() { return primaryDisplayedPredecodeImage(); },
        [this]() { return firstDisplayDecodeContext(); }, dependencies.imageDecode,
        dependencies.cacheBudgets.predecodeCacheByteBudget,
        [this]() { return m_navigationService->currentPageNumber(); },
        [this](const ImageDocumentPageCandidateListContext& context,
            ImageDocumentPageCandidateListSnapshotCallback callback) {
            m_navigationService->ensurePageCandidateSnapshot(context, std::move(callback));
        },
        std::move(dependencies.powerSaver), dependencies.ordinaryDirectMediaPredecodeEnabled,
        std::move(dependencies.predecodeTimerScheduler),
        std::move(dependencies.predecodeThreadCountProvider));
    m_predecodedImageLookup = std::make_unique<ImageDocumentPredecodedImageLookup>(
        *m_predecodeController, std::move(externalPredecodedImageFinder));
    m_spreadController = std::make_unique<ImageSpreadPresentationController>(state,
        ImageSpreadPresentationController::Callbacks {
            [this](
                const std::vector<ImageDocumentChange>& changes) { m_callbacks.notify(changes); },
            [this](const DisplayedImageLocation& location) {
                return m_predecodedImageLookup->find(location);
            },
            [this]() { return m_navigationService->pageNavigationSnapshot(); },
            [this]() {
                dispatchPlan(
                    ImageDocumentRuntimePlan { ScheduleAdjacentImagePredecodeOperation {} });
            },
            [this](const ImageLoadSession& session, std::optional<PredecodedImage> predecoded) {
                prepareViewportSecondaryImageTarget(session, std::move(predecoded));
            },
            [this]() { clearViewportSecondaryImageTarget(); },
            [this]() {
                return m_viewportIntegration->displayedImage(ImageViewportPageRole::Secondary);
            },
        });
    m_openController = std::make_unique<ImageOpenController>(state,
        ImageOpenController::Callbacks {
            [this](const DisplayedImageLocation& location) {
                return m_predecodedImageLookup->find(location);
            },
            [this](const ImageDocumentRuntimePlan& plan) { dispatchPlan(plan); },
            std::move(m_callbacks.unsupportedOpenedCollectionVideoEntered),
            [this](
                const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) {
                MediaEntrySourceVideoPlaybackDeviceResult result
                    = loadOpenedCollectionVideoPlaybackDevice(openedCollectionScope, videoUrl);
                if (const auto* error = kiriview::mediaEntrySourceResultError(result)) {
                    logMediaEntrySourceError(
                        "opened collection video availability probe failed", *error);
                    return false;
                }

                const auto* device = kiriview::mediaEntrySourceResultValue(result);
                return device != nullptr && device->device != nullptr;
            },
            [this](const DisplayedImageLocation& location, QSize imageSize) {
                m_spreadController->commitPrimaryPageSlot(location, imageSize);
            },
            [this]() { m_pendingViewportImageLoad.reset(); },
            [this](const ImageDocumentPageCandidateListContext& context,
                ImageDocumentPageCandidateListSnapshotCallback callback) {
                m_navigationService->ensurePageCandidateSnapshot(context, std::move(callback));
            },
            [this](const ImageLoadSession& session) { return startViewportImageTarget(session); },
            [this](const ImageLoadSession& session, std::optional<PredecodedImage> predecoded) {
                return resolveViewportImageTarget(session, std::move(predecoded));
            },
            [this]() { return firstDisplayDecodeContext(); },
            [this]() { return m_viewportIntegration->hasAuthoritativeDisplay(); },
        });
    m_navigationController
        = std::make_unique<ImageDocumentNavigationController>(state, *m_navigationService,
            *m_spreadController, [this](const ImageDocumentRuntimeTransaction& transaction) {
                dispatchTransaction(transaction);
            });
}

void ImageDocumentRuntimeGraph::composeWorkflowDispatch()
{
    m_runtimeWorkflow
        = std::make_unique<ImageDocumentRuntimeWorkflow>(ImageDocumentRuntimeWorkflowPorts {
            m_state,
            m_mediaEntrySourceStore.get(),
            *m_deletionController,
            [this]() { clearViewportTarget(); },
            [this]() { m_viewportIntegration->stopPlayback(); },
            *m_openController,
            *m_predecodeController,
            *m_spreadController,
            *m_navigationController,
            m_callbacks.loadSource,
            m_callbacks.containerNavigationBoundaryReached,
        });
}

ImageDocumentDeletionController& ImageDocumentRuntimeGraph::deletionController() const
{
    return *m_deletionController;
}

ImageDocumentNavigationController& ImageDocumentRuntimeGraph::navigationController() const
{
    return *m_navigationController;
}

ImageSpreadPresentationController& ImageDocumentRuntimeGraph::spreadController() const
{
    return *m_spreadController;
}

ImageViewportIntegrationRuntime& ImageDocumentRuntimeGraph::viewportIntegration() const
{
    return *m_viewportIntegration;
}

std::optional<DisplayedPredecodeImage>
ImageDocumentRuntimeGraph::primaryDisplayedPredecodeImage() const
{
    if (m_state.status() != ImageDocumentStatus::Ready
        || m_state.sourceKind() != ImageDocumentPageKind::Image
        || m_state.displayedImageLocation().isEmpty()
        || m_viewportIntegration->projection().status != ImageDocumentStatus::Ready
        || m_viewportIntegration->projection().displayedUrl != m_state.displayedUrl()) {
        return std::nullopt;
    }
    std::optional<StaticDisplayImagePayload> displayImage
        = m_viewportIntegration->displayedImage(ImageViewportPageRole::Primary);
    if (!displayImage.has_value()) {
        return std::nullopt;
    }
    return DisplayedPredecodeImage {
        m_state.displayedImageLocation(),
        true,
        std::move(displayImage),
        m_state.embeddedMetadata(),
    };
}

ImageFirstDisplayDecodeContext ImageDocumentRuntimeGraph::firstDisplayDecodeContext() const
{
    const QSizeF viewportSize = m_viewportIntegration->projection().viewportSize;
    if (viewportSize.isEmpty()) {
        return {};
    }
    return { QSize(qCeil(viewportSize.width()), qCeil(viewportSize.height())) };
}

void ImageDocumentRuntimeGraph::requestNextViewportTargetAnchorAtEnd()
{
    m_nextViewportTargetAnchorAtEnd = true;
}

bool ImageDocumentRuntimeGraph::startViewportImageTarget(const ImageLoadSession& session)
{
    auto prepared = std::make_shared<PreparedViewportTargetState>();
    prepared->selectedSession = session;
    prepared->dependencies = m_imageDecodeDependencies;
    prepared->hasAuthoritativeDisplay
        = [this]() { return m_viewportIntegration->hasAuthoritativeDisplay(); };
    prepared->findPredecodedImage = [this](const DisplayedImageLocation& location) {
        return m_predecodedImageLookup->find(location);
    };

    ImageViewportIntegrationTarget target;
    target.sourceGeneration = session.id();
    target.selectedSourceUrl = session.request().sourceUrl();
    target.transitionIntent = session.request().sameScopePageNavigation()
        ? ImageViewportTargetTransitionIntent::SameNavigationScope
        : ImageViewportTargetTransitionIntent::OutsideNavigationScope;
    target.rightToLeft = m_spreadController->rightToLeftReadingEnabled();
    target.anchorAtEnd = std::exchange(m_nextViewportTargetAnchorAtEnd, false);
    const std::shared_ptr<DisplayImageStore> displayStore = m_viewportDisplayStore;
    target.primaryResource = [prepared, displayStore]() {
        prepared->refreshPredecodedImage();
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            prepared->dependencies, prepared->provisionalPreviewPolicy());
        auto resource
            = std::make_shared<ImageViewportProviderResource>(prepared->selectedSession.id(),
                displayScopeIdentityForLocation(prepared->selectedSession.location()), source,
                displayStore, std::make_shared<ImageViewportFailureRegistry>());
        prepared->activeResource = resource;
        prepared->activeSource = source;
        if (prepared->resolvedSession.has_value()) {
            if (!resource->bindDisplayLocationIdentity(
                    displayScopeIdentityForLocation(prepared->resolvedSession->location()))
                || !source->resolveSession(
                    *prepared->resolvedSession, authoritativeSeed(prepared->predecoded))) {
                resource->close();
                return std::shared_ptr<ImageViewportProviderResource> {};
            }
        }
        return resource;
    };

    m_pendingViewportImageLoad.reset();
    m_preparedViewportTarget = prepared;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportSecondaryLoadSession.reset();
    if (m_viewportIntegration->submitTarget(std::move(target))) {
        return true;
    }
    m_preparedViewportTarget.reset();
    m_viewportTarget.reset();
    return false;
}

bool ImageDocumentRuntimeGraph::resolveViewportImageTarget(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    const std::shared_ptr<PreparedViewportTargetState> prepared = m_preparedViewportTarget;
    if (prepared == nullptr || prepared->resolvedSession.has_value()
        || !prepared->selectedSession.sameSession(session) || m_viewportTarget == nullptr
        || m_viewportTarget->sourceGeneration != session.id()) {
        return false;
    }
    if (!m_viewportIntegration->resolvePrimaryTargetUrl(session.id(), session.imageUrl())) {
        return false;
    }
    if (m_preparedViewportTarget != prepared || prepared->resolvedSession.has_value()
        || m_viewportTarget == nullptr || m_viewportTarget->sourceGeneration != session.id()) {
        return false;
    }

    m_viewportTarget->resolvedPrimaryUrl = session.imageUrl();
    prepared->resolvedSession = session;
    prepared->predecoded = std::move(predecoded);
    m_pendingViewportImageLoad = PendingViewportImageLoad {
        session,
        [prepared]() {
            if (const std::shared_ptr<ImageViewportDecodeProviderSource> source
                = prepared->activeSource.lock()) {
                return source->embeddedMetadata();
            }
            return prepared->predecoded.has_value() ? prepared->predecoded->embeddedMetadata
                                                    : EmbeddedMetadata {};
        },
    };

    const ImageViewportIntegrationProjection& projection = m_viewportIntegration->projection();
    if (!projection.correlated) {
        return true;
    }
    if (projection.sourceGeneration != session.id()) {
        return false;
    }

    const std::shared_ptr<ImageViewportProviderResource> resource = prepared->activeResource.lock();
    const std::shared_ptr<ImageViewportDecodeProviderSource> source = prepared->activeSource.lock();
    if (resource == nullptr || source == nullptr) {
        return false;
    }
    if (!resource->bindDisplayLocationIdentity(
            displayScopeIdentityForLocation(session.location()))) {
        return false;
    }
    if (!source->resolveSession(session, authoritativeSeed(prepared->predecoded))) {
        return m_preparedViewportTarget != prepared || m_viewportTarget == nullptr
            || m_viewportTarget->sourceGeneration != session.id();
    }
    return true;
}

void ImageDocumentRuntimeGraph::prepareViewportSecondaryImageTarget(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    if (m_viewportTarget == nullptr || !m_viewportTarget->isValid()) {
        m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        return;
    }

    auto prepared = std::make_shared<PreparedViewportTargetState>();
    prepared->selectedSession = session;
    prepared->resolvedSession = session;
    prepared->dependencies = m_imageDecodeDependencies;
    prepared->predecoded = std::move(predecoded);
    prepared->provisionalPreviewPolicyOverride = ImageViewportProvisionalPreviewPolicy::Suppress;
    prepared->hasAuthoritativeDisplay
        = [this]() { return m_viewportIntegration->hasAuthoritativeDisplay(); };
    prepared->findPredecodedImage = [this](const DisplayedImageLocation& location) {
        return m_predecodedImageLookup->find(location);
    };
    if (m_preparedViewportTarget != nullptr) {
        m_preparedViewportTarget->provisionalPreviewPolicyOverride
            = ImageViewportProvisionalPreviewPolicy::Suppress;
    }
    const std::shared_ptr<DisplayImageStore> displayStore = m_viewportDisplayStore;

    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.secondaryUrl = session.imageUrl();
    target.secondarySessionId = session.id();
    target.transitionIntent = ImageViewportTargetTransitionIntent::PresentationShapeChange;
    target.secondaryResource = [prepared, displayStore]() {
        prepared->refreshPredecodedImage();
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            prepared->dependencies, prepared->provisionalPreviewPolicy());
        prepared->activeSource = source;
        auto resource
            = std::make_shared<ImageViewportProviderResource>(prepared->resolvedSession->id(),
                displayScopeIdentityForLocation(prepared->resolvedSession->location()), source,
                displayStore, std::make_shared<ImageViewportFailureRegistry>());
        if (!source->resolveSession(
                *prepared->resolvedSession, authoritativeSeed(prepared->predecoded))) {
            resource->close();
            return std::shared_ptr<ImageViewportProviderResource> {};
        }
        return resource;
    };
    m_viewportSecondaryLoadSession = session;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    if (!m_viewportIntegration->submitTarget(std::move(target))) {
        if (!m_viewportSecondaryLoadSession.has_value()
            || !m_viewportSecondaryLoadSession->sameSession(session) || m_viewportTarget == nullptr
            || m_viewportTarget->secondarySessionId != session.id()) {
            return;
        }
        m_viewportSecondaryLoadSession.reset();
        m_spreadController->finishViewportSecondaryPageLoadWithError(session);
    }
}

void ImageDocumentRuntimeGraph::clearViewportSecondaryImageTarget()
{
    if (m_viewportTarget == nullptr || m_viewportTarget->secondaryUrl.isEmpty()) {
        m_viewportSecondaryLoadSession.reset();
        return;
    }

    if (m_preparedViewportTarget != nullptr) {
        m_preparedViewportTarget->provisionalPreviewPolicyOverride
            = ImageViewportProvisionalPreviewPolicy::Suppress;
    }
    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.secondaryUrl = QUrl();
    target.secondarySessionId = 0;
    target.secondaryResource = {};
    target.transitionIntent = ImageViewportTargetTransitionIntent::PresentationShapeChange;
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportIntegration->submitTarget(std::move(target));
}

void ImageDocumentRuntimeGraph::clearViewportTarget()
{
    m_preparedViewportTarget.reset();
    m_pendingViewportImageLoad.reset();
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget.reset();
    m_nextViewportTargetAnchorAtEnd = false;
    m_viewportIntegration->clearTarget();
}

void ImageDocumentRuntimeGraph::handleViewportProjection(
    const ImageViewportIntegrationProjection& projection)
{
    const std::optional<ImageLoadSession> observedSecondaryLoad = m_viewportSecondaryLoadSession;
    m_callbacks.notify(
        std::vector<ImageDocumentChange> { ImageDocumentChange::ViewportProjection });
    if (observedSecondaryLoad.has_value() && m_viewportSecondaryLoadSession.has_value()
        && m_viewportSecondaryLoadSession->sameSession(*observedSecondaryLoad)
        && m_viewportTarget != nullptr
        && projection.sourceGeneration == m_viewportTarget->sourceGeneration
        && projection.secondarySessionId == observedSecondaryLoad->id()
        && m_viewportTarget->secondarySessionId == observedSecondaryLoad->id()
        && projection.secondaryUrl == observedSecondaryLoad->imageUrl()
        && m_viewportTarget->secondaryUrl == observedSecondaryLoad->imageUrl()) {
        if (projection.status == ImageDocumentStatus::Ready
            && !projection.secondaryImageSize.isEmpty()) {
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoad(
                *observedSecondaryLoad, projection.secondaryImageSize);
        } else if (projection.status == ImageDocumentStatus::Error) {
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoadWithError(*observedSecondaryLoad);
        }
    }
    if (!m_pendingViewportImageLoad.has_value()
        || projection.sourceGeneration != m_pendingViewportImageLoad->session.id()) {
        return;
    }

    if (projection.status == ImageDocumentStatus::Ready) {
        PendingViewportImageLoad completedLoad = std::move(*m_pendingViewportImageLoad);
        m_pendingViewportImageLoad.reset();
        EmbeddedMetadata metadata
            = completedLoad.metadata ? completedLoad.metadata() : EmbeddedMetadata {};
        m_openController->finishViewportImageLoadReady(
            completedLoad.session, projection.primaryImageSize, std::move(metadata));
        return;
    }
    if (projection.status != ImageDocumentStatus::Error) {
        return;
    }

    qCWarning(kiriviewNavigationLog)
        << "viewport image load failed"
        << "url" << m_pendingViewportImageLoad->session.imageUrl() << "sourceGeneration"
        << projection.sourceGeneration << "displayedUrl" << projection.displayedUrl << "errorString"
        << projection.errorString << "applicationFailureAvailable"
        << projection.failure.has_value();
    if (projection.failure.has_value()) {
        qCWarning(kiriviewNavigationLog)
            << "viewport image load failure detail"
            << "kind" << static_cast<int>(projection.failure->kind) << "decodeRoute"
            << static_cast<int>(projection.failure->decodeRoute) << "decodeOperation"
            << static_cast<int>(projection.failure->decodeOperation) << "diagnosticDetail"
            << projection.failure->diagnosticDetail << "retryable" << projection.failure->retryable;
    }
    PendingViewportImageLoad completedLoad = std::move(*m_pendingViewportImageLoad);
    m_pendingViewportImageLoad.reset();
    ImageLoadFailure failure = projection.failure.value_or(viewportPresentationFailure(
        completedLoad.session, projection.errorString, projection.errorString));
    m_openController->finishViewportImageLoadWithError(completedLoad.session, std::move(failure));
}

MediaEntrySourceVideoPlaybackDeviceResult
ImageDocumentRuntimeGraph::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const
{
    if (m_mediaEntrySourceStore == nullptr) {
        return std::unexpected(MediaEntrySourceError {
            MediaEntrySourceErrorCause::ProviderUnavailable,
            MediaEntrySourceBackendKind::Unknown,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice,
            openedCollectionScope.fileUrl(),
            {},
            QStringLiteral("image document runtime has no media entry source store"),
        });
    }

    return m_mediaEntrySourceStore->loadOpenedCollectionVideoPlaybackDevice(
        openedCollectionScope, videoUrl);
}

void ImageDocumentRuntimeGraph::dispatchPlan(const ImageDocumentRuntimePlan& plan)
{
    const quint64 revision = m_sourceLoadPlanRevision;
    m_runtimeWorkflow->dispatchPlanWhile(
        plan, [this, revision]() { return revision == m_sourceLoadPlanRevision; });
}

void ImageDocumentRuntimeGraph::dispatchSourceLoadPlan(const ImageDocumentRuntimePlan& plan)
{
    const quint64 revision = ++m_sourceLoadPlanRevision;
    m_runtimeWorkflow->dispatchPlanWhile(
        plan, [this, revision]() { return revision == m_sourceLoadPlanRevision; });
}

void ImageDocumentRuntimeGraph::dispatchTransaction(
    const ImageDocumentRuntimeTransaction& transaction)
{
    const quint64 revision = m_sourceLoadPlanRevision;
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    m_callbacks.notify(transaction.changes);
    m_runtimeWorkflow->dispatchPlanWhile(
        transaction.plan, [this, revision]() { return revision == m_sourceLoadPlanRevision; });
}

void ImageDocumentRuntimeGraph::shutdownRuntime()
{
    ++m_sourceLoadPlanRevision;
    m_runtimeWorkflow->shutdownRuntime();
}
}
