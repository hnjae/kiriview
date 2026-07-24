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
    using ChangeCallback = std::function<void(const std::vector<ImageDocumentChange>&)>;
    using FileDeletionFailedCallback = std::function<void(const QString&)>;
    using UnsupportedOpenedCollectionVideoEnteredCallback = std::function<void(const QString&)>;
    using ContainerNavigationBoundaryReachedCallback = std::function<void(const QString&)>;

    ImageDocumentRuntime(QObject* documentObject, ChangeCallback changeCallback,
        ImageDocumentRuntimeDependencyOverrides dependencies,
        FileDeletionFailedCallback fileDeletionFailedCallback = {},
        UnsupportedOpenedCollectionVideoEnteredCallback
            unsupportedOpenedCollectionVideoEnteredCallback
        = {},
        ContainerNavigationBoundaryReachedCallback containerNavigationBoundaryReachedCallback = {});
    ~ImageDocumentRuntime();
    Q_DISABLE_COPY_MOVE(ImageDocumentRuntime)

    [[nodiscard]] QUrl sourceUrl() const;
    [[nodiscard]] ImageDocumentPageKind sourceKind() const;
    void setSourceUrl(const QUrl& sourceUrl);
    void setSource(const ResolvedNavigationSource& source);
    [[nodiscard]] MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const;
    [[nodiscard]] ImageDocumentStatus status() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] const std::optional<ImageLoadFailure>& loadFailure() const;
    [[nodiscard]] QString windowTitleFileName() const;
    [[nodiscard]] QUrl displayedUrl() const;
    [[nodiscard]] OpenedCollectionScopeLocation displayedOpenedCollectionScope() const;
    [[nodiscard]] QSize imageSize() const;
    [[nodiscard]] QSize primaryImageSize() const;
    [[nodiscard]] QSize secondaryImageSize() const;
    [[nodiscard]] bool viewportHorizontallyPannable() const;
    [[nodiscard]] bool viewportVerticallyPannable() const;
    [[nodiscard]] bool viewportPannable() const;
    [[nodiscard]] qreal horizontalScrollPosition() const;
    [[nodiscard]] qreal horizontalScrollPageSize() const;
    [[nodiscard]] qreal verticalScrollPosition() const;
    [[nodiscard]] qreal verticalScrollPageSize() const;
    bool submitHorizontalScrollPosition(qreal position);
    bool submitVerticalScrollPosition(qreal position);
    [[nodiscard]] bool zoomPercentKnown() const;
    [[nodiscard]] qreal zoomPercent() const;
    bool requestManualZoomPercentAtCenter(qreal zoomPercent);
    bool requestZoomByStep(qreal stepCount, QPointF viewportAnchorPoint);
    bool requestZoomByStepAtCenter(qreal stepCount);
    bool requestToggleFitOrActualSize(QPointF viewportPoint);
    [[nodiscard]] ImageZoomMode zoomMode() const;
    [[nodiscard]] ImageZoomMode fitModeSelection() const;
    [[nodiscard]] qreal maximumManualZoomPercent() const;
    [[nodiscard]] qreal clampedManualZoomPercent(qreal zoomPercent) const;
    [[nodiscard]] qreal steppedManualZoomPercent(qreal stepCount) const;
    [[nodiscard]] int rotationDegrees() const;
    quint64 requestViewportPanBy(QPointF delta);
    quint64 requestViewportPanToInitialScanPosition();
    quint64 requestViewportPanToFinalScanPosition();
    quint64 requestViewportScanForward();
    quint64 requestViewportScanBackward();
    void requestNextViewportTargetAnchorAtEnd();
    [[nodiscard]] int currentPageNumber() const;
    [[nodiscard]] int currentLastPageNumber() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    [[nodiscard]] const ImageDocumentPageCandidateListSnapshot&
    confirmedPageCandidateSnapshot() const;
    [[nodiscard]] ImageDocumentPageActiveNavigationSnapshot activeNavigationSnapshot() const;
    [[nodiscard]] bool containerNavigationAvailable() const;
    [[nodiscard]] bool ordinaryDirectMediaScopeActive() const;
    [[nodiscard]] bool openedCollectionScopeActive() const;
    [[nodiscard]] bool fileDeletionInProgress() const;
    [[nodiscard]] bool twoPageModeEnabled() const;
    void setTwoPageModeEnabled(bool enabled);
    [[nodiscard]] bool twoPageModeAvailable() const;
    [[nodiscard]] bool rightToLeftReadingEnabled() const;
    void setRightToLeftReadingEnabled(bool enabled);
    [[nodiscard]] bool rightToLeftReadingAvailable() const;
    [[nodiscard]] bool secondaryPageVisible() const;
    [[nodiscard]] QPointF nearestImageViewportPoint(QPointF viewportPoint) const;
    [[nodiscard]] bool unsupportedOpenedCollectionVideo() const;
    [[nodiscard]] std::optional<DisplayedPredecodeImage> primaryDisplayedPredecodeImage() const;
    [[nodiscard]] ImageFirstDisplayDecodeContext firstDisplayDecodeContext() const;
    [[nodiscard]] const EmbeddedMetadata& embeddedMetadata() const;
    void attachImageViewport(ImageViewport* viewport);
    void detachImageViewport(ImageViewport* viewport);
    [[nodiscard]] const ImageViewportIntegrationProjection& viewportProjection() const;

    void notify(const std::vector<ImageDocumentChange>& changes);
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
    [[nodiscard]] QPointF scanPosition(bool forward) const;
    [[nodiscard]] QPointF scanBoundaryPosition(bool final) const;
    bool submitContentPosition(QPointF position);
    bool requestAnchoredManualZoom(qreal zoomPercent, QPointF viewportAnchorPoint);
    void loadSource(const ImageDocumentSourceLoadRequest& request);
    void publishChanges(const std::vector<ImageDocumentChange>& changes);

    ImageDocumentChangeBatcher changeBatcher;
    ImageDocumentState state;
    ChangeCallback changeCallback;
    NavigationSourceResolver navigationSourceResolver;
    std::unique_ptr<ImageDocumentRuntimeGraph> runtimeGraph;
    ImageZoomMode fitModeSelectionPreference = ImageZoomMode::Fit;
};
}

#endif
