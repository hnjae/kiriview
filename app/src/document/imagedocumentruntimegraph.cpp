// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimegraph.h"

#include "archive/mediaentrysourcestore.h"
#include "diagnostics/diagnosticlogprojection.h"
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
#include "localization/imageerrortext.h"
#include "navigation/imagedocumentpagenavigationservice.h"
#include "navigation/navigationlogging.h"
#include "presentation/imagespreadpresentationcontroller.h"
#include "rendering/displayimagestore.h"
#include "rendering/imageviewportdecodesource.h"

#include <QObject>
#include <QUrl>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

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

    std::optional<QSize> providerLogicalSize(const ImageViewportProviderMetadataResult& result)
    {
        if (!result.metadata.has_value() || !result.metadata->isValid()) {
            return std::nullopt;
        }
        const QSizeF logicalSize = result.metadata->sourceLogicalSize();
        if (!std::isfinite(logicalSize.width()) || !std::isfinite(logicalSize.height())
            || logicalSize.width() <= 0.0 || logicalSize.height() <= 0.0) {
            return std::nullopt;
        }
        const QSize size = logicalSize.toSize();
        return size.isEmpty() ? std::nullopt : std::optional<QSize>(size);
    }
}

struct ImageDocumentRuntimeGraph::PreparedViewportTargetState
{
    struct ActiveProvider
    {
        std::weak_ptr<ImageViewportProviderResource> resource;
        std::weak_ptr<ImageViewportDecodeProviderSource> source;
    };

    ImageLoadSession selectedSession;
    std::optional<ImageLoadSession> resolvedSession;
    ImageDecodeDependencies dependencies;
    std::optional<PredecodedImage> predecoded;
    std::optional<ImageViewportProvisionalPreviewPolicy> provisionalPreviewPolicyOverride;
    std::function<bool()> hasAuthoritativeDisplay;
    std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>
        findPredecodedImage;
    std::vector<ActiveProvider> activeProviders;

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

    void pruneActiveProviders()
    {
        std::erase_if(activeProviders, [](const ActiveProvider& provider) {
            return provider.resource.expired() || provider.source.expired();
        });
    }

    [[nodiscard]] EmbeddedMetadata embeddedMetadata()
    {
        pruneActiveProviders();
        for (const ActiveProvider& provider : activeProviders) {
            if (const std::shared_ptr<ImageViewportDecodeProviderSource> source
                = provider.source.lock()) {
                const EmbeddedMetadata& metadata = source->embeddedMetadata();
                if (!metadata.isEmpty()) {
                    return metadata;
                }
            }
        }
        return predecoded.has_value() ? predecoded->embeddedMetadata : EmbeddedMetadata {};
    }

    bool resolveActiveProviders()
    {
        if (!resolvedSession.has_value()) {
            return true;
        }
        pruneActiveProviders();
        const QString displayIdentity
            = displayScopeIdentityForLocation(resolvedSession->location());
        const std::optional<StaticDisplayImagePayload> seed = authoritativeSeed(predecoded);
        for (const ActiveProvider& provider : activeProviders) {
            const std::shared_ptr<ImageViewportProviderResource> resource
                = provider.resource.lock();
            const std::shared_ptr<ImageViewportDecodeProviderSource> source
                = provider.source.lock();
            if (resource == nullptr || source == nullptr
                || !resource->bindDisplayLocationIdentity(displayIdentity)
                || !source->resolveSession(*resolvedSession, seed)) {
                for (const ActiveProvider& active : activeProviders) {
                    if (const std::shared_ptr<ImageViewportProviderResource> activeResource
                        = active.resource.lock()) {
                        activeResource->close();
                    }
                }
                return false;
            }
        }
        return true;
    }

    std::shared_ptr<ImageViewportProviderResource> makeResource(
        const std::shared_ptr<DisplayImageStore>& displayStore)
    {
        refreshPredecodedImage();
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            dependencies, provisionalPreviewPolicy());
        auto resource = std::make_shared<ImageViewportProviderResource>(selectedSession.id(),
            displayScopeIdentityForLocation(selectedSession.location()), source, displayStore,
            std::make_shared<ImageViewportFailureRegistry>(), dependencies.workspaceBudget);
        if (resolvedSession.has_value()
            && (!resource->bindDisplayLocationIdentity(
                    displayScopeIdentityForLocation(resolvedSession->location()))
                || !source->resolveSession(*resolvedSession, authoritativeSeed(predecoded)))) {
            resource->close();
            return {};
        }
        pruneActiveProviders();
        activeProviders.push_back({ resource, source });
        return resource;
    }
};

struct ImageDocumentRuntimeGraph::PreparedViewportRole
{
    using FindPredecodedImageCallback
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;

    ImageLoadSession session;
    ImageDecodeDependencies dependencies;
    std::optional<PredecodedImage> predecoded;
    FindPredecodedImageCallback findPredecodedImage;
    std::shared_ptr<ImageViewportDecodeProviderSource> stagedSource;
    std::weak_ptr<ImageViewportDecodeProviderSource> activeSource;

    void refreshPredecodedImage()
    {
        if (!findPredecodedImage) {
            return;
        }
        std::optional<PredecodedImage> candidate = findPredecodedImage(session.location());
        if (candidate.has_value() && candidate->location == session.location()
            && candidate->displayImage.isAuthoritative()) {
            predecoded = std::move(candidate);
        }
    }

    bool prepare()
    {
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            dependencies, ImageViewportProvisionalPreviewPolicy::Suppress);
        if (!source->resolveSession(session, authoritativeSeed(predecoded))) {
            source->close();
            return false;
        }
        stagedSource = std::move(source);
        return true;
    }

    [[nodiscard]] EmbeddedMetadata embeddedMetadata() const
    {
        if (const std::shared_ptr<ImageViewportDecodeProviderSource> source = activeSource.lock()) {
            return source->embeddedMetadata();
        }
        if (stagedSource != nullptr) {
            return stagedSource->embeddedMetadata();
        }
        return predecoded.has_value() ? predecoded->embeddedMetadata : EmbeddedMetadata {};
    }

    std::shared_ptr<ImageViewportProviderResource> makeResource(
        const std::shared_ptr<DisplayImageStore>& displayStore)
    {
        std::shared_ptr<ImageViewportDecodeProviderSource> source = std::exchange(stagedSource, {});
        if (source == nullptr) {
            refreshPredecodedImage();
            source = std::make_shared<ImageViewportDecodeProviderSource>(
                dependencies, ImageViewportProvisionalPreviewPolicy::Suppress);
            if (!source->resolveSession(session, authoritativeSeed(predecoded))) {
                source->close();
                return {};
            }
        }

        auto resource = std::make_shared<ImageViewportProviderResource>(session.id(),
            displayScopeIdentityForLocation(session.location()), source, displayStore,
            std::make_shared<ImageViewportFailureRegistry>(), dependencies.workspaceBudget);
        activeSource = source;
        return resource;
    }
};

struct ImageDocumentRuntimeGraph::PendingSpreadReplacement
{
    enum class Phase {
        PreparingPrimaryMetadata,
        PlanningPairing,
        PreparingSecondaryMetadata,
        FinalSubmitted,
    };

    ImageLoadSession selectedSession;
    std::shared_ptr<PreparedViewportTargetState> admissionTarget;
    ImageViewportIntegrationTarget admissionViewportTarget;
    std::shared_ptr<PreparedViewportRole> primary;
    std::shared_ptr<PreparedViewportRole> secondary;
    QSize primaryImageSize;
    QSize secondaryImageSize;
    Phase phase = Phase::PreparingPrimaryMetadata;
    bool expectsSecondary = false;
    ImageViewportTargetTransitionIntent finalTransitionIntent
        = ImageViewportTargetTransitionIntent::SameNavigationScope;
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
        dependencies.powerSaver, dependencies.ordinaryDirectMediaPredecodeEnabled,
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
            [this](quint64 primarySessionId, const ImageLoadSession& secondarySession,
                std::optional<PredecodedImage> predecoded) {
                prepareSpreadReplacementSecondary(
                    primarySessionId, secondarySession, std::move(predecoded));
            },
            [this](quint64 primarySessionId, const ImageLoadSession&, ImageLoadFailure failure) {
                const std::shared_ptr<PendingSpreadReplacement> replacement
                    = m_pendingSpreadReplacement;
                if (replacement != nullptr
                    && replacement->selectedSession.id() == primarySessionId) {
                    failSpreadReplacement(replacement, std::move(failure),
                        QStringLiteral("spread secondary source preparation failed"));
                }
            },
            [this]() {
                cancelPendingViewportImageLoad();
                clearViewportSecondaryImageTarget();
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
            [this](const ImageLoadSession& session, QSize imageSize) {
                return commitViewportPresentation(session, imageSize);
            },
            [this]() { cancelPendingViewportImageLoad(); },
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
    cancelPendingViewportImageLoad();

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
    target.primaryResource
        = [prepared, displayStore]() { return prepared->makeResource(displayStore); };

    m_preparedViewportTarget = prepared;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportSecondaryLoadSession.reset();
    if (session.request().sameScopePageNavigation()
        && session.kind() == ImageDocumentPageKind::Image
        && session.openedCollectionScope().isComicBook()
        && m_spreadController->twoPageModeActive()) {
        auto replacement = std::make_shared<PendingSpreadReplacement>();
        replacement->selectedSession = session;
        replacement->admissionTarget = prepared;
        replacement->admissionViewportTarget = target;
        m_pendingSpreadReplacement = std::move(replacement);
    }
    if (m_viewportIntegration->submitTarget(std::move(target))) {
        return true;
    }
    cancelPendingViewportImageLoad();
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
    const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
    if (replacement != nullptr && replacement->admissionTarget == prepared
        && replacement->selectedSession.sameSession(session)
        && replacement->phase == PendingSpreadReplacement::Phase::PreparingPrimaryMetadata) {
        replacement->admissionViewportTarget.resolvedPrimaryUrl = session.imageUrl();
        auto primary = std::make_shared<PreparedViewportRole>();
        primary->session = session;
        primary->dependencies = m_imageDecodeDependencies;
        primary->predecoded = std::move(predecoded);
        primary->findPredecodedImage = [this](const DisplayedImageLocation& location) {
            return m_predecodedImageLookup->find(location);
        };
        if (!primary->prepare()) {
            cancelPendingViewportImageLoad();
            return false;
        }

        replacement->primary = primary;
        m_pendingViewportImageLoad = PendingViewportImageLoad {
            session,
            [primary]() { return primary->embeddedMetadata(); },
        };
        const std::weak_ptr<PendingSpreadReplacement> weakReplacement = replacement;
        primary->stagedSource->requestMetadata(
            ImageViewportProviderWorkIdentity {
                session.id(),
                ImageViewportPageRole::Primary,
                {},
                {},
                displayScopeIdentityForLocation(session.location()),
            },
            [this, weakReplacement](const ImageViewportProviderWorkIdentity&,
                ImageViewportProviderMetadataResult result) mutable {
                const std::shared_ptr<PendingSpreadReplacement> current = weakReplacement.lock();
                if (current != nullptr && m_pendingSpreadReplacement == current) {
                    finishSpreadReplacementPrimaryMetadata(current, std::move(result));
                }
            });
        return true;
    }

    prepared->resolvedSession = session;
    prepared->predecoded = std::move(predecoded);
    m_pendingViewportImageLoad = PendingViewportImageLoad {
        session,
        [prepared]() { return prepared->embeddedMetadata(); },
    };

    const ImageViewportIntegrationProjection& projection = m_viewportIntegration->projection();
    if (!projection.correlated) {
        return true;
    }
    if (projection.sourceGeneration != session.id()) {
        return false;
    }

    prepared->pruneActiveProviders();
    if (prepared->activeProviders.empty()) {
        return false;
    }
    if (!prepared->resolveActiveProviders()) {
        return m_preparedViewportTarget != prepared || m_viewportTarget == nullptr
            || m_viewportTarget->sourceGeneration != session.id();
    }
    return true;
}

bool ImageDocumentRuntimeGraph::submitSpreadReplacementTarget(
    const std::shared_ptr<PendingSpreadReplacement>& replacement, bool includeSecondary)
{
    if (replacement == nullptr || m_pendingSpreadReplacement != replacement
        || replacement->primary == nullptr || replacement->primaryImageSize.isEmpty()
        || m_viewportTarget == nullptr || !m_viewportTarget->isValid()
        || m_viewportTarget->sourceGeneration != replacement->selectedSession.id()
        || (includeSecondary
            && (replacement->secondary == nullptr || replacement->secondaryImageSize.isEmpty()))) {
        return false;
    }

    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.transitionIntent = replacement->finalTransitionIntent;
    target.rightToLeft = m_spreadController->rightToLeftReadingEnabled();
    const std::shared_ptr<DisplayImageStore> displayStore = m_viewportDisplayStore;
    const std::shared_ptr<PreparedViewportRole> primary = replacement->primary;
    target.primaryResource
        = [primary, displayStore]() { return primary->makeResource(displayStore); };
    if (includeSecondary) {
        const std::shared_ptr<PreparedViewportRole> secondary = replacement->secondary;
        target.secondaryUrl = secondary->session.imageUrl();
        target.secondarySessionId = secondary->session.id();
        target.secondaryResource
            = [secondary, displayStore]() { return secondary->makeResource(displayStore); };
    } else {
        target.secondaryUrl = {};
        target.secondarySessionId = 0;
        target.secondaryResource = {};
    }

    replacement->expectsSecondary = includeSecondary;
    replacement->phase = PendingSpreadReplacement::Phase::FinalSubmitted;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    const bool submitted = m_viewportIntegration->submitTarget(std::move(target));
    return m_pendingSpreadReplacement != replacement || submitted;
}

bool ImageDocumentRuntimeGraph::submitSpreadReplacementAdmissionTarget(
    const std::shared_ptr<PendingSpreadReplacement>& replacement)
{
    if (replacement == nullptr || m_pendingSpreadReplacement != replacement
        || !replacement->admissionViewportTarget.isValid()
        || replacement->admissionViewportTarget.sourceGeneration
            != replacement->selectedSession.id()
        || (!replacement->admissionViewportTarget.resolvedPrimaryUrl.isEmpty()
            && replacement->admissionViewportTarget.resolvedPrimaryUrl
                != replacement->selectedSession.imageUrl())) {
        return false;
    }

    ImageViewportIntegrationTarget target = replacement->admissionViewportTarget;
    target.rightToLeft = m_spreadController->rightToLeftReadingEnabled();
    target.anchorAtEnd = false;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    const bool submitted = m_viewportIntegration->submitTarget(std::move(target));
    return m_pendingSpreadReplacement != replacement || submitted;
}

void ImageDocumentRuntimeGraph::prepareSpreadReplacementSecondary(quint64 primarySessionId,
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
    if (replacement == nullptr || replacement->selectedSession.id() != primarySessionId
        || replacement->phase != PendingSpreadReplacement::Phase::PlanningPairing
        || session.id() == 0 || session.kind() != ImageDocumentPageKind::Image) {
        return;
    }

    auto secondary = std::make_shared<PreparedViewportRole>();
    secondary->session = session;
    secondary->dependencies = m_imageDecodeDependencies;
    secondary->predecoded = std::move(predecoded);
    secondary->findPredecodedImage = [this](const DisplayedImageLocation& location) {
        return m_predecodedImageLookup->find(location);
    };
    if (!secondary->prepare()) {
        failSpreadReplacement(replacement, std::nullopt,
            QStringLiteral("spread secondary provider preparation failed"));
        return;
    }

    replacement->secondary = secondary;
    replacement->phase = PendingSpreadReplacement::Phase::PreparingSecondaryMetadata;
    const std::weak_ptr<PendingSpreadReplacement> weakReplacement = replacement;
    secondary->stagedSource->requestMetadata(
        ImageViewportProviderWorkIdentity {
            session.id(),
            ImageViewportPageRole::Secondary,
            {},
            {},
            displayScopeIdentityForLocation(session.location()),
        },
        [this, weakReplacement, session](const ImageViewportProviderWorkIdentity&,
            ImageViewportProviderMetadataResult result) mutable {
            const std::shared_ptr<PendingSpreadReplacement> current = weakReplacement.lock();
            if (current != nullptr && m_pendingSpreadReplacement == current) {
                finishSpreadReplacementSecondaryMetadata(current, session, std::move(result));
            }
        });
}

void ImageDocumentRuntimeGraph::finishSpreadReplacementPrimaryMetadata(
    const std::shared_ptr<PendingSpreadReplacement>& replacement,
    ImageViewportProviderMetadataResult result)
{
    if (replacement == nullptr || m_pendingSpreadReplacement != replacement
        || replacement->phase != PendingSpreadReplacement::Phase::PreparingPrimaryMetadata
        || replacement->primary == nullptr) {
        return;
    }

    const std::optional<QSize> imageSize = providerLogicalSize(result);
    if (!imageSize.has_value()) {
        failSpreadReplacement(replacement, std::move(result.failure),
            QStringLiteral("spread primary metadata is unavailable"));
        return;
    }

    replacement->primaryImageSize = *imageSize;
    replacement->phase = PendingSpreadReplacement::Phase::PlanningPairing;
    const ImageSpreadPageReplacementPairingResult pairing
        = m_spreadController->beginPageReplacementPairing(
            replacement->primary->session, replacement->primaryImageSize);
    if (m_pendingSpreadReplacement != replacement) {
        return;
    }
    switch (pairing) {
    case ImageSpreadPageReplacementPairingResult::PreparingSecondary:
        return;
    case ImageSpreadPageReplacementPairingResult::PrimaryOnly:
        if (submitSpreadReplacementTarget(replacement, false)) {
            return;
        }
        failSpreadReplacement(replacement, std::nullopt,
            QStringLiteral("spread primary-only target submission failed"));
        return;
    case ImageSpreadPageReplacementPairingResult::Stale:
        failSpreadReplacement(
            replacement, std::nullopt, QStringLiteral("spread primary pairing became stale"));
        return;
    }
}

void ImageDocumentRuntimeGraph::finishSpreadReplacementSecondaryMetadata(
    const std::shared_ptr<PendingSpreadReplacement>& replacement,
    const ImageLoadSession& secondarySession, ImageViewportProviderMetadataResult result)
{
    if (replacement == nullptr || m_pendingSpreadReplacement != replacement
        || replacement->phase != PendingSpreadReplacement::Phase::PreparingSecondaryMetadata
        || replacement->secondary == nullptr
        || !replacement->secondary->session.sameSession(secondarySession)
        || replacement->secondary->session.location() != secondarySession.location()) {
        return;
    }

    [[maybe_unused]] const std::shared_ptr<PreparedViewportRole> preparedSecondary
        = replacement->secondary;
    const std::optional<QSize> imageSize = providerLogicalSize(result);
    if (!imageSize.has_value()) {
        failSpreadReplacement(replacement, std::move(result.failure),
            QStringLiteral("spread secondary metadata is unavailable"));
        return;
    }

    const ImageSpreadPageReplacementSecondaryMetadataResult pairing
        = m_spreadController->finishPageReplacementSecondaryMetadata(
            replacement->selectedSession.id(), secondarySession, *imageSize);
    if (m_pendingSpreadReplacement != replacement) {
        return;
    }
    switch (pairing) {
    case ImageSpreadPageReplacementSecondaryMetadataResult::Secondary:
        replacement->secondaryImageSize = *imageSize;
        if (submitSpreadReplacementTarget(replacement, true)) {
            return;
        }
        failSpreadReplacement(
            replacement, std::nullopt, QStringLiteral("spread target submission failed"));
        return;
    case ImageSpreadPageReplacementSecondaryMetadataResult::PrimaryOnly:
        replacement->secondary.reset();
        replacement->secondaryImageSize = {};
        if (submitSpreadReplacementTarget(replacement, false)) {
            return;
        }
        failSpreadReplacement(replacement, std::nullopt,
            QStringLiteral("spread primary-only target submission failed"));
        return;
    case ImageSpreadPageReplacementSecondaryMetadataResult::Stale:
        failSpreadReplacement(
            replacement, std::nullopt, QStringLiteral("spread secondary pairing became stale"));
        return;
    }
}

bool ImageDocumentRuntimeGraph::commitViewportPresentation(
    const ImageLoadSession& primarySession, QSize primaryImageSize)
{
    const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
    if (replacement == nullptr) {
        m_spreadController->commitPrimaryPageSlot(primarySession.location(), primaryImageSize);
        return true;
    }

    if (replacement->phase != PendingSpreadReplacement::Phase::FinalSubmitted
        || replacement->primary == nullptr
        || !replacement->selectedSession.sameSession(primarySession)
        || replacement->selectedSession.location() != primarySession.location()
        || replacement->primaryImageSize != primaryImageSize) {
        return false;
    }

    std::optional<ImageSpreadPreparedSecondaryPage> secondary;
    if (replacement->expectsSecondary) {
        if (replacement->secondary == nullptr || replacement->secondaryImageSize.isEmpty()) {
            return false;
        }
        secondary = ImageSpreadPreparedSecondaryPage {
            replacement->secondary->session,
            replacement->secondaryImageSize,
        };
    }
    if (!m_spreadController->commitPageReplacementPresentation(
            primarySession, primaryImageSize, secondary)) {
        return false;
    }
    m_pendingSpreadReplacement.reset();
    return true;
}

void ImageDocumentRuntimeGraph::failSpreadReplacement(
    const std::shared_ptr<PendingSpreadReplacement>& replacement,
    std::optional<ImageLoadFailure> failure, const QString& diagnosticDetail)
{
    if (replacement == nullptr || m_pendingSpreadReplacement != replacement) {
        return;
    }

    const ImageLoadSession primarySession = replacement->selectedSession;
    const QString userMessage = imageErrorText(ImageErrorTextId::DecodeImageAnimation);
    ImageLoadFailure reportedFailure = failure.has_value()
        ? std::move(*failure)
        : viewportPresentationFailure(primarySession, userMessage, diagnosticDetail);
    cancelPendingViewportImageLoad();
    m_openController->finishViewportImageLoadWithError(primarySession, std::move(reportedFailure));
}

void ImageDocumentRuntimeGraph::cancelPendingViewportImageLoad()
{
    const std::shared_ptr<PendingSpreadReplacement> replacement
        = std::exchange(m_pendingSpreadReplacement, {});
    m_pendingViewportImageLoad.reset();
    if (replacement != nullptr && m_spreadController != nullptr) {
        m_spreadController->cancelPageReplacementPairing(replacement->selectedSession.id());
    }
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
        auto resource
            = std::make_shared<ImageViewportProviderResource>(prepared->resolvedSession->id(),
                displayScopeIdentityForLocation(prepared->resolvedSession->location()), source,
                displayStore, std::make_shared<ImageViewportFailureRegistry>(),
                prepared->dependencies.workspaceBudget);
        if (!source->resolveSession(
                *prepared->resolvedSession, authoritativeSeed(prepared->predecoded))) {
            resource->close();
            return std::shared_ptr<ImageViewportProviderResource> {};
        }
        prepared->pruneActiveProviders();
        prepared->activeProviders.push_back({ resource, source });
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
    const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
    if (replacement != nullptr) {
        replacement->finalTransitionIntent
            = ImageViewportTargetTransitionIntent::PresentationShapeChange;
        replacement->expectsSecondary = false;
        [[maybe_unused]] const std::shared_ptr<PreparedViewportRole> retiredSecondary
            = std::exchange(replacement->secondary, {});
        replacement->secondaryImageSize = {};
        m_viewportSecondaryLoadSession.reset();

        if (replacement->primary != nullptr && !replacement->primaryImageSize.isEmpty()) {
            replacement->phase = PendingSpreadReplacement::Phase::PlanningPairing;
            if (!submitSpreadReplacementAdmissionTarget(replacement)) {
                failSpreadReplacement(replacement, std::nullopt,
                    QStringLiteral("explicit spread clear could not restore target admission"));
                return;
            }
            if (m_pendingSpreadReplacement != replacement) {
                return;
            }
            const ImageSpreadPageReplacementPairingResult pairing
                = m_spreadController->beginPageReplacementPairing(
                    replacement->primary->session, replacement->primaryImageSize);
            if (m_pendingSpreadReplacement != replacement) {
                return;
            }
            if (pairing == ImageSpreadPageReplacementPairingResult::PreparingSecondary) {
                return;
            }
            if (pairing == ImageSpreadPageReplacementPairingResult::PrimaryOnly) {
                if (submitSpreadReplacementTarget(replacement, false)) {
                    return;
                }
                failSpreadReplacement(replacement, std::nullopt,
                    QStringLiteral("explicit spread clear could not submit primary-only target"));
                return;
            }
            failSpreadReplacement(replacement, std::nullopt,
                QStringLiteral("explicit spread clear pairing became stale"));
            return;
        }

        if (!submitSpreadReplacementAdmissionTarget(replacement)
            && m_pendingSpreadReplacement == replacement) {
            failSpreadReplacement(replacement, std::nullopt,
                QStringLiteral("explicit spread clear admission submission failed"));
        }
        return;
    }

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
    cancelPendingViewportImageLoad();
    m_preparedViewportTarget.reset();
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget.reset();
    m_nextViewportTargetAnchorAtEnd = false;
    m_viewportIntegration->clearTarget();
}

void ImageDocumentRuntimeGraph::handleViewportProjection(
    const ImageViewportIntegrationProjection& projection)
{
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
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
        const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
        if (replacement != nullptr
            && replacement->selectedSession.id() == projection.sourceGeneration) {
            const bool primaryMatches
                = replacement->phase == PendingSpreadReplacement::Phase::FinalSubmitted
                && replacement->primary != nullptr && !projection.primaryImageSize.isEmpty()
                && projection.primaryImageSize == replacement->primaryImageSize
                && projection.displayedUrl == replacement->primary->session.imageUrl();
            bool rolesMatch = primaryMatches;
            if (rolesMatch && replacement->expectsSecondary) {
                rolesMatch = replacement->secondary != nullptr
                    && projection.secondarySessionId == replacement->secondary->session.id()
                    && projection.secondaryUrl == replacement->secondary->session.imageUrl()
                    && projection.secondaryVisible && !projection.secondaryImageSize.isEmpty()
                    && projection.secondaryImageSize == replacement->secondaryImageSize;
            } else if (rolesMatch) {
                rolesMatch = projection.secondarySessionId == 0 && projection.secondaryUrl.isEmpty()
                    && !projection.secondaryVisible && projection.secondaryImageSize.isEmpty();
            }
            if (!rolesMatch) {
                failSpreadReplacement(replacement, std::nullopt,
                    QStringLiteral("spread target committed an unexpected role set"));
                return;
            }
        }

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
        << "url" << diagnosticSourceReference(m_pendingViewportImageLoad->session.imageUrl())
        << "sourceGeneration" << projection.sourceGeneration << "displayedUrl"
        << diagnosticSourceReference(projection.displayedUrl) << "errorString"
        << diagnosticDetailReference(projection.errorString) << "applicationFailureAvailable"
        << projection.failure.has_value();
    if (projection.failure.has_value()) {
        qCWarning(kiriviewNavigationLog)
            << "viewport image load failure detail"
            << "kind" << static_cast<int>(projection.failure->kind) << "decodeRoute"
            << static_cast<int>(projection.failure->decodeRoute) << "decodeOperation"
            << static_cast<int>(projection.failure->decodeOperation) << "decodeCause"
            << static_cast<int>(projection.failure->decodeCause) << "diagnosticDetail"
            << diagnosticDetailReference(projection.failure->diagnosticDetail) << "retryable"
            << projection.failure->retryable;
    }
    PendingViewportImageLoad completedLoad = std::move(*m_pendingViewportImageLoad);
    m_pendingViewportImageLoad.reset();
    const std::shared_ptr<PendingSpreadReplacement> replacement = m_pendingSpreadReplacement;
    if (replacement != nullptr && replacement->selectedSession.sameSession(completedLoad.session)) {
        m_pendingSpreadReplacement.reset();
        m_spreadController->cancelPageReplacementPairing(replacement->selectedSession.id());
    }
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
