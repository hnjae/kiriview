// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTRUNTIMEGRAPH_H
#define KIRIVIEW_IMAGEDOCUMENTRUNTIMEGRAPH_H

#include "archive/mediaentrysourcebackend.h"
#include "imagedocumentruntimedependencies.h"
#include "imagedocumentruntimeplan.h"
#include "imagedocumenttypes.h"
#include "imageloadtypes.h"
#include "location/imageurl.h"
#include "metadata/embeddedmetadata.h"
#include "predecode/predecodedimage.h"
#include "rendering/imagerendercontext.h"

#include <QString>
#include <functional>
#include <memory>
#include <vector>

class QObject;

namespace kiriview {
class MediaEntrySourceStore;
class DisplayImageStore;
class ImageDocumentAdjacentPredecodeSchedulerPort;
class ImageDocumentCurrentPageNumberPort;
class ImageDocumentDeletionController;
class ImageDocumentDeletionProgressPort;
class ImageDocumentNavigationSnapshotPort;
class ImageDocumentPageCandidateSnapshotPort;
class ImageDocumentPredecodedImageLookup;
class ImageDocumentPrimaryPageSlotPort;
class ImageDocumentRuntimeWorkflow;
class ImageDocumentNavigationController;
class ImageDocumentPredecodeController;
class ImageDocumentState;
class ImageDocumentSourceLoadRequest;
class ImageDocumentPageNavigationService;
class ImageOpenController;
class ImageSpreadPresentationController;
class ImageViewportIntegrationRuntime;
struct ImageViewportIntegrationTarget;
struct ImageViewportIntegrationProjection;

struct ImageDocumentRuntimeGraphCallbacks
{
    std::function<ImageDocumentRenderContext()> renderContext;
    std::function<void(const std::vector<ImageDocumentChange>&)> notify;
    std::function<void(const ImageDocumentSourceLoadRequest&)> loadSource;
    std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource;
    std::function<void(const QString&)> fileDeletionFailed;
    std::function<void(const QString&)> unsupportedOpenedCollectionVideoEntered;
    std::function<void(const QString&)> containerNavigationBoundaryReached;
};

class ImageDocumentRuntimeGraph final
{
public:
    ImageDocumentRuntimeGraph(QObject* documentObject, ImageDocumentState& state,
        ImageDocumentRuntimeDependencyOverrides dependencies,
        ImageDocumentRuntimeGraphCallbacks callbacks);
    ~ImageDocumentRuntimeGraph();

    ImageDocumentDeletionController& deletionController() const;
    ImageDocumentNavigationController& navigationController() const;
    ImageSpreadPresentationController& spreadController() const;
    ImageViewportIntegrationRuntime& viewportIntegration() const;
    std::optional<DisplayedPredecodeImage> primaryDisplayedPredecodeImage() const;
    ImageFirstDisplayDecodeContext firstDisplayDecodeContext() const;
    MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const;

    void dispatchPlan(const ImageDocumentRuntimePlan& plan);
    void dispatchTransaction(const ImageDocumentRuntimeTransaction& transaction);
    void shutdownRuntime();

private:
    void composeSurfaceAndPresentation(
        QObject* documentObject, ImageDocumentRuntimeDependencies& dependencies);
    void composeNavigationAndCandidatePorts(
        QObject* documentObject, ImageDocumentRuntimeDependencies& dependencies);
    void composeWorkflowOwners(QObject* documentObject, ImageDocumentState& state,
        ImageDocumentRuntimeDependencies& dependencies,
        ExternalPredecodedImageFinder externalPredecodedImageFinder);
    void composeWorkflowDispatch(ImageDocumentState& state);
    bool prepareViewportImageTarget(
        ImageLoadSession session, std::optional<PredecodedImage> predecoded);
    void prepareViewportSecondaryImageTarget(ImageLoadSession session,
        std::optional<PredecodedImage> predecoded, bool priorTwoPageModeEnabled);
    void clearViewportSecondaryImageTarget(bool priorTwoPageModeEnabled);
    void clearViewportTarget();
    void handleViewportProjection(const ImageViewportIntegrationProjection& projection);

    ImageDocumentRuntimeGraphCallbacks m_callbacks;
    ImageDocumentState& m_state;
    std::unique_ptr<MediaEntrySourceStore> m_mediaEntrySourceStore;
    std::unique_ptr<ImageDocumentDeletionController> m_deletionController;
    std::unique_ptr<ImageDocumentDeletionProgressPort> m_deletionProgressPort;
    std::shared_ptr<DisplayImageStore> m_viewportDisplayStore;
    std::unique_ptr<ImageViewportIntegrationRuntime> m_viewportIntegration;
    std::unique_ptr<ImageDocumentPageNavigationService> m_navigationService;
    std::unique_ptr<ImageDocumentNavigationSnapshotPort> m_navigationSnapshotPort;
    std::unique_ptr<ImageDocumentCurrentPageNumberPort> m_currentPageNumberPort;
    std::unique_ptr<ImageDocumentPageCandidateSnapshotPort> m_pageCandidateSnapshotPort;
    std::unique_ptr<ImageDocumentAdjacentPredecodeSchedulerPort> m_adjacentPredecodeSchedulerPort;
    std::unique_ptr<ImageDocumentPredecodeController> m_predecodeController;
    std::unique_ptr<ImageDocumentPredecodedImageLookup> m_predecodedImageLookup;
    std::unique_ptr<ImageSpreadPresentationController> m_spreadController;
    std::unique_ptr<ImageDocumentPrimaryPageSlotPort> m_primaryPageSlotPort;
    std::unique_ptr<ImageOpenController> m_openController;
    std::unique_ptr<ImageDocumentNavigationController> m_navigationController;
    std::unique_ptr<ImageDocumentRuntimeWorkflow> m_runtimeWorkflow;
    ImageDecodeDependencies m_imageDecodeDependencies;
    std::optional<ImageLoadSession> m_viewportLoadSession;
    std::function<EmbeddedMetadata()> m_viewportMetadata;
    bool m_viewportLoadTerminal = false;
    std::unique_ptr<ImageViewportIntegrationTarget> m_viewportTarget;
    std::optional<ImageLoadSession> m_viewportSecondaryLoadSession;
};
}

#endif
