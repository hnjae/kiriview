// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimegraph.h"

#include "archive/mediaentrysourcestore.h"
#include "async/imagecallback.h"
#include "imagedocumentadjacentpredecodeschedulerport.h"
#include "imagedocumentcurrentpagenumberport.h"
#include "imagedocumentdeletioncontroller.h"
#include "imagedocumentdeletionprogressport.h"
#include "imagedocumentnavigationcontroller.h"
#include "imagedocumentnavigationruntimeplan.h"
#include "imagedocumentnavigationsnapshotport.h"
#include "imagedocumentpagecandidatesnapshotport.h"
#include "imagedocumentpredecodecontroller.h"
#include "imagedocumentpredecodedimagelookup.h"
#include "imagedocumentprimarypageslotport.h"
#include "imagedocumentruntimedependencies.h"
#include "imagedocumentruntimeworkflow.h"
#include "imagedocumentsourceloadrequest.h"
#include "imagedocumentstate.h"
#include "imageopencontroller.h"
#include "imageviewportintegrationruntime.h"
#include "location/sourcekey.h"
#include "navigation/imagedocumentpagenavigationservice.h"
#include "presentation/imagespreadpresentationcontroller.h"
#include "rendering/displayimagestore.h"
#include "rendering/imageviewportdecodesource.h"

#include <QObject>
#include <QUrl>
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
    ImageDocumentRuntimeDependencies runtimeDependencies
        = resolveImageDocumentRuntimeDependencies(std::move(dependencies), documentObject);
    ExternalPredecodedImageFinder externalPredecodedImageFinder
        = std::move(runtimeDependencies.externalPredecodedImageFinder);

    composeSurfaceAndPresentation(documentObject, runtimeDependencies);
    composeNavigationAndCandidatePorts(documentObject, runtimeDependencies);
    composeWorkflowOwners(
        documentObject, state, runtimeDependencies, std::move(externalPredecodedImageFinder));
    composeWorkflowDispatch(state);
}

ImageDocumentRuntimeGraph::~ImageDocumentRuntimeGraph() = default;

void ImageDocumentRuntimeGraph::composeSurfaceAndPresentation(
    QObject* documentObject, ImageDocumentRuntimeDependencies& dependencies)
{
    m_viewportDisplayStore = std::make_shared<DisplayImageStore>(
        dependencies.cacheBudgets.displayImageCacheByteBudget);
    m_viewportIntegration = std::make_unique<ImageViewportIntegrationRuntime>(
        ImageViewportIntegrationRuntime::Callbacks {
            [this](const ImageViewportIntegrationProjection& projection) {
                handleViewportProjection(projection);
            },
            [this](bool enabled) {
                if (m_spreadController != nullptr) {
                    m_spreadController->restoreTwoPageModeEnabled(enabled);
                }
            },
        },
        documentObject);
}

void ImageDocumentRuntimeGraph::composeNavigationAndCandidatePorts(
    QObject* documentObject, ImageDocumentRuntimeDependencies& dependencies)
{
    m_navigationService = std::make_unique<ImageDocumentPageNavigationService>(documentObject,
        dependencies.candidateProvider,
        ImageDocumentPageNavigationService::Callbacks {
            [this](ImageDocumentPageNavigationPlan plan) {
                dispatchPlan(imageDocumentRuntimePlanForNavigationPlan(
                    plan, m_state.displayedOpenedCollectionScope()));
            },
            [this](ImageDocumentPageNavigationCommit commit) {
                dispatchTransaction(ImageDocumentRuntimeTransaction {
                    commit.pageNavigationChanged
                        ? std::vector<ImageDocumentChange> { ImageDocumentChange::PageNavigation }
                        : std::vector<ImageDocumentChange> {},
                    imageDocumentRuntimePlanForNavigationPlan(
                        commit.effects, m_state.displayedOpenedCollectionScope()),
                });
            },
            [this]() {
                return m_deletionProgressPort != nullptr && m_deletionProgressPort->inProgress();
            },
            m_callbacks.resolveExternalSource,
        });
    m_navigationSnapshotPort
        = std::make_unique<ImageDocumentNavigationSnapshotPort>(m_navigationService.get());
    m_currentPageNumberPort
        = std::make_unique<ImageDocumentCurrentPageNumberPort>(m_navigationService.get());
    m_pageCandidateSnapshotPort
        = std::make_unique<ImageDocumentPageCandidateSnapshotPort>(m_navigationService.get());
    m_adjacentPredecodeSchedulerPort
        = std::make_unique<ImageDocumentAdjacentPredecodeSchedulerPort>(
            [this](const ImageDocumentRuntimePlan& plan) { dispatchPlan(plan); });
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
            return m_viewportIntegration != nullptr
                && m_viewportIntegration->projection().correlated
                && !m_viewportIntegration->projection().displayedUrl.isEmpty();
        },
        dependencies.candidateProvider, std::move(dependencies.fileDeletionProvider),
        ImageDocumentDeletionController::Callbacks {
            [this]() {
                invokeIfSet(m_callbacks.notify,
                    std::vector<ImageDocumentChange> {
                        ImageDocumentChange::FileDeletionInProgress });
            },
            [this](ImageDocumentRuntimePlan plan) { dispatchPlan(plan); },
            std::move(m_callbacks.fileDeletionFailed),
        },
        m_callbacks.resolveExternalSource);
    m_deletionProgressPort
        = std::make_unique<ImageDocumentDeletionProgressPort>(m_deletionController.get());
    m_predecodeController = std::make_unique<ImageDocumentPredecodeController>(
        documentObject, state, [this]() { return primaryDisplayedPredecodeImage(); },
        [this]() { return firstDisplayDecodeContext(); }, dependencies.imageDecode,
        dependencies.cacheBudgets.predecodeCacheByteBudget,
        [this]() { return m_currentPageNumberPort->currentPageNumber(); },
        [this](ImageDocumentPageCandidateListContext context,
            ImageDocumentPageCandidateListSnapshotCallback callback) {
            m_pageCandidateSnapshotPort->ensure(std::move(context), std::move(callback));
        },
        std::move(dependencies.powerSaver), dependencies.ordinaryDirectMediaPredecodeEnabled,
        std::move(dependencies.predecodeTimerScheduler),
        std::move(dependencies.predecodeThreadCountProvider));
    m_predecodedImageLookup = std::make_unique<ImageDocumentPredecodedImageLookup>(
        std::move(externalPredecodedImageFinder), m_predecodeController.get());
    m_spreadController = std::make_unique<ImageSpreadPresentationController>(documentObject, state,
        ImageSpreadPresentationController::Callbacks {
            [this](const std::vector<ImageDocumentChange>& changes) {
                invokeIfSet(m_callbacks.notify, changes);
            },
            [this](const QUrl& url) { return m_predecodedImageLookup->find(url); },
            [this]() { return m_navigationSnapshotPort->snapshot(); },
            [this]() { m_adjacentPredecodeSchedulerPort->scheduleAdjacentImagePredecode(); },
            [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded,
                bool priorTwoPageModeEnabled) {
                prepareViewportSecondaryImageTarget(
                    std::move(session), std::move(predecoded), priorTwoPageModeEnabled);
            },
            [this](bool priorTwoPageModeEnabled) {
                clearViewportSecondaryImageTarget(priorTwoPageModeEnabled);
            },
            [this]() {
                return m_viewportIntegration->displayedImage(ImageViewportPageRole::Secondary);
            },
        });
    m_primaryPageSlotPort
        = std::make_unique<ImageDocumentPrimaryPageSlotPort>(m_spreadController.get());
    m_openController = std::make_unique<ImageOpenController>(documentObject, state,
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
                const auto* device = std::get_if<MediaEntrySourceVideoPlaybackDevice>(&result);
                return device != nullptr && device->device != nullptr;
            },
            [this](const DisplayedImageLocation& location, QSize imageSize) {
                m_primaryPageSlotPort->commit(location, imageSize);
            },
            [this]() { m_primaryPageSlotPort->clear(); },
            [this](ImageDocumentPageCandidateListContext context,
                ImageDocumentPageCandidateListSnapshotCallback callback) {
                m_pageCandidateSnapshotPort->ensure(std::move(context), std::move(callback));
            },
            [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded) {
                return prepareViewportImageTarget(std::move(session), std::move(predecoded));
            },
            [this]() { return firstDisplayDecodeContext(); },
            [this]() {
                return m_viewportIntegration != nullptr
                    && !m_viewportIntegration->projection().displayedUrl.isEmpty();
            },
            [this]() { clearViewportTarget(); },
        });
    m_navigationController = std::make_unique<ImageDocumentNavigationController>(state,
        *m_navigationService, *m_spreadController,
        [this](ImageDocumentRuntimeTransaction transaction) { dispatchTransaction(transaction); });
}

void ImageDocumentRuntimeGraph::composeWorkflowDispatch(ImageDocumentState& state)
{
    m_runtimeWorkflow
        = std::make_unique<ImageDocumentRuntimeWorkflow>(ImageDocumentRuntimeWorkflowPorts {
            &state,
            m_mediaEntrySourceStore.get(),
            m_deletionController.get(),
            [this]() { clearViewportTarget(); },
            m_openController.get(),
            m_predecodeController.get(),
            m_spreadController.get(),
            m_navigationController.get(),
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
    if (m_viewportIntegration == nullptr || m_state.displayedImageLocation().isEmpty()) {
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
    if (m_viewportIntegration == nullptr) {
        return {};
    }
    const QSizeF viewportSize = m_viewportIntegration->projection().viewportSize;
    if (viewportSize.isEmpty()) {
        return {};
    }
    const ImageDocumentRenderContext context
        = m_callbacks.renderContext ? m_callbacks.renderContext() : ImageDocumentRenderContext {};
    const qreal devicePixelRatio = context.devicePixelRatio > 0.0 ? context.devicePixelRatio : 1.0;
    return { QSize(qCeil(viewportSize.width() * devicePixelRatio),
        qCeil(viewportSize.height() * devicePixelRatio)) };
}

bool ImageDocumentRuntimeGraph::prepareViewportImageTarget(
    ImageLoadSession session, std::optional<PredecodedImage> predecoded)
{
    if (m_viewportIntegration == nullptr) {
        return false;
    }

    auto prepared = std::make_shared<PreparedViewportTargetState>();
    prepared->session = session;
    prepared->dependencies = m_imageDecodeDependencies;
    prepared->predecoded = std::move(predecoded);

    ImageViewportIntegrationTarget target;
    target.sourceGeneration = session.id();
    target.primaryUrl = session.imageUrl();
    target.navigationScopeIdentity = displayScopeIdentityForLocation(session.location());
    target.transitionIntent = session.request().sameScopePageNavigation()
        ? ImageViewportTargetTransitionIntent::SameNavigationScope
        : session.request().preserveTwoPageSpreadTransition()
        ? ImageViewportTargetTransitionIntent::RetainedDirectImage
        : ImageViewportTargetTransitionIntent::OutsideNavigationScope;
    target.rightToLeft
        = m_spreadController != nullptr && m_spreadController->rightToLeftReadingEnabled();
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
            sourceKeyForUrl(prepared->session.imageUrl()).identity, std::move(source), displayStore,
            std::make_shared<ImageViewportFailureRegistry>(), std::move(predecodedImage));
    };

    m_viewportLoadSession = session;
    m_viewportLoadTerminal = false;
    m_viewportMetadata = [prepared]() {
        if (prepared->source != nullptr) {
            return prepared->source->embeddedMetadata();
        }
        return prepared->predecoded.has_value() ? prepared->predecoded->embeddedMetadata
                                                : EmbeddedMetadata {};
    };
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportSecondaryLoadSession.reset();
    return m_viewportIntegration->submitTarget(std::move(target));
}

void ImageDocumentRuntimeGraph::prepareViewportSecondaryImageTarget(ImageLoadSession session,
    std::optional<PredecodedImage> predecoded, bool priorTwoPageModeEnabled)
{
    if (m_viewportIntegration == nullptr || m_viewportTarget == nullptr
        || !m_viewportTarget->isValid()) {
        if (m_spreadController != nullptr) {
            m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        }
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
    target.priorTwoPageModeEnabled = priorTwoPageModeEnabled;
    target.secondaryResource = [prepared, displayStore]() {
        auto source = std::make_shared<ImageViewportDecodeProviderSource>(
            prepared->session, prepared->dependencies);
        prepared->source = source;
        std::optional<StaticDisplayImagePayload> predecodedImage;
        if (prepared->predecoded.has_value()) {
            predecodedImage = prepared->predecoded->displayImage;
        }
        return std::make_shared<ImageViewportProviderResource>(prepared->session.id(),
            sourceKeyForUrl(prepared->session.imageUrl()).identity, std::move(source), displayStore,
            std::make_shared<ImageViewportFailureRegistry>(), std::move(predecodedImage));
    };
    m_viewportSecondaryLoadSession = session;
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    if (!m_viewportIntegration->submitTarget(std::move(target))) {
        m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        m_viewportSecondaryLoadSession.reset();
    }
}

void ImageDocumentRuntimeGraph::clearViewportSecondaryImageTarget(bool priorTwoPageModeEnabled)
{
    if (m_viewportIntegration == nullptr || m_viewportTarget == nullptr
        || m_viewportTarget->secondaryUrl.isEmpty()) {
        m_viewportSecondaryLoadSession.reset();
        return;
    }

    ImageViewportIntegrationTarget target = *m_viewportTarget;
    target.secondaryUrl = QUrl();
    target.secondaryResource = {};
    target.transitionIntent = ImageViewportTargetTransitionIntent::PresentationShapeChange;
    target.priorTwoPageModeEnabled = priorTwoPageModeEnabled;
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget = std::make_unique<ImageViewportIntegrationTarget>(target);
    m_viewportIntegration->submitTarget(std::move(target));
}

void ImageDocumentRuntimeGraph::clearViewportTarget()
{
    m_viewportLoadSession.reset();
    m_viewportSecondaryLoadSession.reset();
    m_viewportTarget.reset();
    m_viewportMetadata = {};
    m_viewportLoadTerminal = false;
    if (m_viewportIntegration != nullptr) {
        m_viewportIntegration->clearTarget();
    }
}

void ImageDocumentRuntimeGraph::handleViewportProjection(
    const ImageViewportIntegrationProjection& projection)
{
    invokeIfSet(m_callbacks.notify,
        std::vector<ImageDocumentChange> { ImageDocumentChange::ViewportProjection });
    if (m_spreadController != nullptr && m_viewportSecondaryLoadSession.has_value()
        && m_viewportTarget != nullptr
        && projection.sourceGeneration == m_viewportTarget->sourceGeneration
        && projection.secondaryUrl == m_viewportSecondaryLoadSession->imageUrl()) {
        if (projection.restoredTransition) {
            const ImageLoadSession session = *m_viewportSecondaryLoadSession;
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        } else if (projection.status == ImageDocumentStatus::Ready
            && !projection.secondaryImageSize.isEmpty()) {
            const ImageLoadSession session = *m_viewportSecondaryLoadSession;
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoad(
                session, projection.secondaryImageSize, false);
        } else if (projection.status == ImageDocumentStatus::Error) {
            const ImageLoadSession session = *m_viewportSecondaryLoadSession;
            m_viewportSecondaryLoadSession.reset();
            m_spreadController->finishViewportSecondaryPageLoadWithError(session);
        }
    }
    if (m_openController == nullptr || !m_viewportLoadSession.has_value()
        || projection.sourceGeneration != m_viewportLoadSession->id() || m_viewportLoadTerminal) {
        return;
    }

    if (projection.status == ImageDocumentStatus::Ready) {
        m_viewportLoadTerminal = true;
        m_openController->finishViewportImageLoadReady(*m_viewportLoadSession,
            projection.primaryImageSize,
            m_viewportMetadata ? m_viewportMetadata() : EmbeddedMetadata {});
        return;
    }
    if (projection.status != ImageDocumentStatus::Error) {
        return;
    }

    m_viewportLoadTerminal = true;
    ImageLoadFailure failure = projection.failure.value_or(viewportPresentationFailure(
        *m_viewportLoadSession, projection.errorString, projection.errorString));
    m_openController->finishViewportImageLoadWithError(*m_viewportLoadSession, std::move(failure));
}

MediaEntrySourceVideoPlaybackDeviceResult
ImageDocumentRuntimeGraph::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const
{
    if (m_mediaEntrySourceStore == nullptr) {
        return MediaEntrySourceError {
            MediaEntrySourceBackendKind::Unsupported,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice,
            openedCollectionScope.fileUrl(),
            QString(),
            QString(),
            QString(),
        };
    }

    return m_mediaEntrySourceStore->loadOpenedCollectionVideoPlaybackDevice(
        openedCollectionScope, videoUrl);
}

void ImageDocumentRuntimeGraph::dispatchPlan(const ImageDocumentRuntimePlan& plan)
{
    if (m_runtimeWorkflow != nullptr) {
        m_runtimeWorkflow->dispatchPlan(plan);
    }
}

void ImageDocumentRuntimeGraph::dispatchTransaction(
    const ImageDocumentRuntimeTransaction& transaction)
{
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    invokeIfSet(m_callbacks.notify, transaction.changes);
    dispatchPlan(transaction.plan);
}

void ImageDocumentRuntimeGraph::shutdownRuntime()
{
    if (m_runtimeWorkflow != nullptr) {
        m_runtimeWorkflow->shutdownRuntime();
    }
}
}
