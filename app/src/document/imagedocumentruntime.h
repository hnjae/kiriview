// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTRUNTIME_H
#define KIRIVIEW_IMAGEDOCUMENTRUNTIME_H

#include "archive/mediaentrysourcebackend.h"
#include "imagedocumentruntimedependencies.h"
#include "imagedocumentstate.h"
#include "imagedocumenttypes.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "navigation/imagedocumentpagenavigationtypes.h"
#include "predecode/predecodedimage.h"
#include "presentation/imagepresentationstate.h"
#include "presentation/imageviewportscanstate.h"
#include "rendering/imagerendercontext.h"
#include "system/filedeletion.h"

#include <QPointF>
#include <QSize>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QObject;
class ImageViewport;

namespace kiriview {
class ImageDocumentRuntimeGraph;
class ImageDocumentSourceLoadRequest;
struct ImageViewportIntegrationProjection;

class ImageDocumentRuntime final
{
public:
    using RenderContextProvider = std::function<ImageDocumentRenderContext()>;
    using ChangeCallback = std::function<void(const std::vector<ImageDocumentChange>&)>;
    using FileDeletionFailedCallback = std::function<void(const QString&)>;
    using UnsupportedOpenedCollectionVideoEnteredCallback = std::function<void(const QString&)>;
    using ContainerNavigationBoundaryReachedCallback = std::function<void(const QString&)>;

    ImageDocumentRuntime(QObject* documentObject, RenderContextProvider renderContextProvider,
        ChangeCallback changeCallback, ImageDocumentRuntimeDependencyOverrides dependencies,
        FileDeletionFailedCallback fileDeletionFailedCallback = {},
        UnsupportedOpenedCollectionVideoEnteredCallback
            unsupportedOpenedCollectionVideoEnteredCallback
        = {},
        ContainerNavigationBoundaryReachedCallback containerNavigationBoundaryReachedCallback = {});
    ~ImageDocumentRuntime();

    QUrl sourceUrl() const;
    ImageDocumentPageKind sourceKind() const;
    void setSourceUrl(const QUrl& sourceUrl);
    void setSource(const ResolvedNavigationSource& source);
    void setExternalSourcePreservingPresentation(const ResolvedNavigationSource& source);
    MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const;
    ImageDocumentStatus status() const;
    bool loading() const;
    QString errorString() const;
    const std::optional<ImageLoadFailure>& loadFailure() const;
    QString windowTitleFileName() const;
    QUrl displayedUrl() const;
    OpenedCollectionScopeLocation displayedOpenedCollectionScope() const;
    QSize imageSize() const;
    QSize primaryImageSize() const;
    QSize secondaryImageSize() const;
    bool viewportHorizontallyPannable() const;
    bool viewportVerticallyPannable() const;
    bool viewportPannable() const;
    qreal horizontalScrollPosition() const;
    qreal horizontalScrollPageSize() const;
    qreal verticalScrollPosition() const;
    qreal verticalScrollPageSize() const;
    bool submitHorizontalScrollPosition(qreal position);
    bool submitVerticalScrollPosition(qreal position);
    bool zoomPercentKnown() const;
    qreal zoomPercent() const;
    bool requestManualZoomPercentAtCenter(qreal zoomPercent);
    bool requestZoomByStep(qreal stepCount, QPointF viewportAnchorPoint);
    bool requestZoomByStepAtCenter(qreal stepCount);
    bool requestActualSizeAtCenter();
    bool requestToggleFitOrActualSize(QPointF viewportPoint);
    ImageZoomMode zoomMode() const;
    ImageZoomMode fitModeSelection() const;
    qreal maximumManualZoomPercent() const;
    qreal clampedManualZoomPercent(qreal zoomPercent) const;
    qreal steppedManualZoomPercent(qreal stepCount) const;
    int rotationDegrees() const;
    quint64 requestViewportPanBy(QPointF delta);
    quint64 requestViewportPanToInitialScanPosition();
    quint64 requestViewportPanToFinalScanPosition();
    quint64 requestViewportScanForward();
    quint64 requestViewportScanBackward();
    void requestNextDisplayedImageStartToFinalScanPosition();
    quint64 requestDisplayedImageInitialContentPosition();
    int currentPageNumber() const;
    int currentLastPageNumber() const;
    int pageCount() const;
    ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    const ImageDocumentPageCandidateListSnapshot& confirmedPageCandidateSnapshot() const;
    ImageDocumentPageActiveNavigationSnapshot activeNavigationSnapshot() const;
    bool containerNavigationAvailable() const;
    bool ordinaryDirectMediaScopeActive() const;
    bool openedCollectionScopeActive() const;
    bool fileDeletionInProgress() const;
    bool twoPageModeEnabled() const;
    void setTwoPageModeEnabled(bool enabled);
    bool twoPageModeAvailable() const;
    bool rightToLeftReadingEnabled() const;
    void setRightToLeftReadingEnabled(bool enabled);
    bool rightToLeftReadingAvailable() const;
    bool secondaryPageVisible() const;
    ImagePresentationTransitionState presentationTransitionState() const;
    bool viewportPointInsideImage(QPointF viewportPoint) const;
    QPointF nearestImageViewportPoint(QPointF viewportPoint) const;
    bool unsupportedOpenedCollectionVideo() const;
    std::optional<DisplayedPredecodeImage> primaryDisplayedPredecodeImage() const;
    ImageFirstDisplayDecodeContext firstDisplayDecodeContext() const;
    const EmbeddedMetadata& embeddedMetadata() const;
    void attachImageViewport(ImageViewport* viewport);
    void detachImageViewport(ImageViewport* viewport);
    const ImageViewportIntegrationProjection& viewportProjection() const;

    void notify(const std::vector<ImageDocumentChange>& changes);
    void setRenderContextProvider(RenderContextProvider provider);
    void shutdown();
    void openPreviousPage();
    void openNextPage();
    void openPreviousSinglePage();
    void openNextSinglePage();
    void openImageAtPage(int pageNumber);
    void openPreviousContainer();
    void openNextContainer();
    void deleteDisplayedFile(FileDeletionMode mode);
    void resetZoom();
    void setFitMode(ImageZoomMode zoomMode);
    void rotateClockwise();
    void rotateCounterclockwise();

private:
    ImageDocumentRenderContext renderContext() const;
    QPointF scanPosition(bool forward) const;
    QPointF scanBoundaryPosition(bool final) const;
    bool submitContentPosition(QPointF position);
    bool requestAnchoredManualZoom(qreal zoomPercent, QPointF viewportAnchorPoint);
    void loadSource(const ImageDocumentSourceLoadRequest& request);
    void publishChanges(const std::vector<ImageDocumentChange>& changes);

    ImageDocumentChangeBatcher changeBatcher;
    ImageDocumentState state;
    ChangeCallback changeCallback;
    RenderContextProvider renderContextProvider;
    NavigationSourceResolver navigationSourceResolver;
    std::unique_ptr<ImageDocumentRuntimeGraph> runtimeGraph;
    ImageViewportScanState viewportScanState;
    QUrl lastProjectedDisplayedUrl;
    ImageZoomMode fitModeSelectionPreference = ImageZoomMode::Fit;
};
}

#endif
