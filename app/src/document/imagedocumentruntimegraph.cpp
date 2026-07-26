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

    struct PreparedViewportTargetState
    {
        ImageLoadSession session;
        ImageDecodeDependencies dependencies;
        std::optional<PredecodedImage> predecoded;
        std::shared_ptr<ImageViewportDecodeProviderSource> source;
    };
}

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
            [this](const QUrl& url) { return m_predecodedImageLookup->find(url); },
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
            [this](const QUrl& url) { return m_predecodedImageLookup->find(url); },
            [this](const ImageDocumentRuntimePlan& plan) { dispatchPlan(plan); },
            std::move(m_callbacks.unsupportedOpenedCollectionVideoEntered),
            [this](
                const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) {
                if (m_mediaEntrySourceStore == nullptr) {
                    return false;
                }

                MediaEntrySourceVideoPlaybackDeviceResult result
                    = m_mediaEntrySourceStore->loadOpenedCollectionVideoPlaybackDevice(
                        openedCollectionScope, videoUrl);
                const auto* device = kiriview::mediaEntrySourceResultValue(result);
                return device != nullptr && device->device != nullptr;
            },
            [this](const DisplayedImageLocation& location, QSize imageSize) {
                m_spreadController->commitPrimaryPageSlot(location, imageSize);
            },
            [this](const ImageDocumentPageCandidateListContext& context,
                ImageDocumentPageCandidateListSnapshotCallback callback) {
                m_navigationService->ensurePageCandidateSnapshot(context, std::move(callback));
            },
            [this](const ImageLoadSession& session, std::optional<PredecodedImage> predecoded) {
                return prepareViewportImageTarget(session, std::move(predecoded));
            },
            [this]() { return firstDisplayDecodeContext(); },
            [this]() { return !m_viewportIntegration->projection().displayedUrl.isEmpty(); },
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

bool ImageDocumentRuntimeGraph::prepareViewportImageTarget(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    auto prepared = std::make_shared<PreparedViewportTargetState>();
    prepared->session = session;
    prepared->dependencies = m_imageDecodeDependencies;
    prepared->predecoded = std::move(predecoded);

    ImageViewportIntegrationTarget target;
    target.sourceGeneration = session.id();
    target.primaryUrl = session.imageUrl();
    target.transitionIntent = session.request().sameScopePageNavigation()
        ? ImageViewportTargetTransitionIntent::SameNavigationScope
        : ImageViewportTargetTransitionIntent::OutsideNavigationScope;
    target.rightToLeft = m_spreadController->rightToLeftReadingEnabled();
    target.anchorAtEnd = std::exchange(m_nextViewportTargetAnchorAtEnd, false);
    const std::shared_ptr<DisplayImageStore> displayStore = m_viewportDisplayStore;
    target.primaryResource = [prepared, displayStore]() {
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            prepared->session, prepared->dependencies);
        prepared->source = source;
        std::optional<StaticDisplayImagePayload> predecodedImage;
        if (prepared->predecoded.has_value()) {
            predecodedImage = prepared->predecoded->displayImage;
        }
        return std::make_shared<ImageViewportProviderResource>(prepared->session.id(),
            displayScopeIdentityForLocation(prepared->session.location()), std::move(source),
            displayStore, std::make_shared<ImageViewportFailureRegistry>(),
            std::move(predecodedImage));
    };

    m_pendingViewportImageLoad = PendingViewportImageLoad {
        session,
        [prepared]() {
            if (prepared->source != nullptr) {
                return prepared->source->embeddedMetadata();
            }
            return prepared->predecoded.has_value() ? prepared->predecoded->embeddedMetadata
                                                    : EmbeddedMetadata {};
        },
    };
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportSecondaryLoadSession.reset();
    return m_viewportIntegration->submitTarget(std::move(target));
}

void ImageDocumentRuntimeGraph::prepareViewportSecondaryImageTarget(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    if (m_viewportTarget == nullptr || !m_viewportTarget->isValid()) {
        m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        return;
    }

    auto prepared = std::make_shared<PreparedViewportTargetState>();
    prepared->session = session;
    prepared->dependencies = m_imageDecodeDependencies;
    prepared->predecoded = std::move(predecoded);
    const std::shared_ptr<DisplayImageStore> displayStore = m_viewportDisplayStore;

    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.secondaryUrl = session.imageUrl();
    target.transitionIntent = ImageViewportTargetTransitionIntent::PresentationShapeChange;
    target.secondaryResource = [prepared, displayStore]() {
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            prepared->session, prepared->dependencies);
        prepared->source = source;
        std::optional<StaticDisplayImagePayload> predecodedImage;
        if (prepared->predecoded.has_value()) {
            predecodedImage = prepared->predecoded->displayImage;
        }
        return std::make_shared<ImageViewportProviderResource>(prepared->session.id(),
            displayScopeIdentityForLocation(prepared->session.location()), std::move(source),
            displayStore, std::make_shared<ImageViewportFailureRegistry>(),
            std::move(predecodedImage));
    };
    m_viewportSecondaryLoadSession = session;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    if (!m_viewportIntegration->submitTarget(std::move(target))) {
        m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        m_viewportSecondaryLoadSession.reset();
    }
}

void ImageDocumentRuntimeGraph::clearViewportSecondaryImageTarget()
{
    if (m_viewportTarget == nullptr || m_viewportTarget->secondaryUrl.isEmpty()) {
        m_viewportSecondaryLoadSession.reset();
        return;
    }

    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.secondaryUrl = QUrl();
    target.secondaryResource = {};
    target.transitionIntent = ImageViewportTargetTransitionIntent::PresentationShapeChange;
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportIntegration->submitTarget(std::move(target));
}

void ImageDocumentRuntimeGraph::clearViewportTarget()
{
    m_pendingViewportImageLoad.reset();
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget.reset();
    m_nextViewportTargetAnchorAtEnd = false;
    m_viewportIntegration->clearTarget();
}

void ImageDocumentRuntimeGraph::handleViewportProjection(
    const ImageViewportIntegrationProjection& projection)
{
    m_callbacks.notify(
        std::vector<ImageDocumentChange> { ImageDocumentChange::ViewportProjection });
    if (m_viewportSecondaryLoadSession.has_value() && m_viewportTarget != nullptr
        && projection.sourceGeneration == m_viewportTarget->sourceGeneration
        && projection.secondaryUrl == m_viewportSecondaryLoadSession->imageUrl()) {
        if (projection.status == ImageDocumentStatus::Ready
            && !projection.secondaryImageSize.isEmpty()) {
            const ImageLoadSession session = *m_viewportSecondaryLoadSession;
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoad(
                session, projection.secondaryImageSize);
        } else if (projection.status == ImageDocumentStatus::Error) {
            const ImageLoadSession session = *m_viewportSecondaryLoadSession;
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoadWithError(session);
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
            MediaEntrySourceBackendKind::Unsupported,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice,
            openedCollectionScope.fileUrl(),
            QString(),
            QString(),
            QString(),
        });
    }

    return m_mediaEntrySourceStore->loadOpenedCollectionVideoPlaybackDevice(
        openedCollectionScope, videoUrl);
}

void ImageDocumentRuntimeGraph::dispatchPlan(const ImageDocumentRuntimePlan& plan)
{
    m_runtimeWorkflow->dispatchPlan(plan);
}

void ImageDocumentRuntimeGraph::dispatchTransaction(
    const ImageDocumentRuntimeTransaction& transaction)
{
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    m_callbacks.notify(transaction.changes);
    dispatchPlan(transaction.plan);
}

void ImageDocumentRuntimeGraph::shutdownRuntime() { m_runtimeWorkflow->shutdownRuntime(); }
}
