// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntime.h"

#include "async/imagecallback.h"
#include "imagedocumentdeletioncontroller.h"
#include "imagedocumentnavigationcontroller.h"
#include "imagedocumentruntimegraph.h"
#include "imagedocumentsourceloadrequest.h"
#include "imageopenworkflow.h"
#include "imageviewportintegrationruntime.h"
#include "presentation/imagespreadpresentationcontroller.h"

#include <QObject>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr qreal scanStepViewportRatio = 0.875;
constexpr qreal positionEpsilon = 0.001;

bool pointIsFinite(QPointF point) { return std::isfinite(point.x()) && std::isfinite(point.y()); }

bool projectionActive(const kiriview::ImageViewportIntegrationProjection& projection)
{
    return projection.sourceGeneration != 0;
}

bool projectionMatchesReadyImage(const kiriview::ImageDocumentState& state,
    const kiriview::ImageViewportIntegrationProjection& projection)
{
    return state.status() == kiriview::ImageDocumentStatus::Ready
        && state.sourceKind() == kiriview::ImageDocumentPageKind::Image
        && projectionActive(projection) && projection.correlated
        && projection.status == kiriview::ImageDocumentStatus::Ready
        && !state.displayedUrl().isEmpty() && projection.displayedUrl == state.displayedUrl();
}

kiriview::ImageZoomMode imageZoomMode(ImageViewportFitMode fitMode)
{
    switch (fitMode) {
    case ImageViewportFitMode::Contain:
        return kiriview::ImageZoomMode::Fit;
    case ImageViewportFitMode::FitWidth:
        return kiriview::ImageZoomMode::FitWidth;
    case ImageViewportFitMode::FitHeight:
        return kiriview::ImageZoomMode::FitHeight;
    case ImageViewportFitMode::Manual:
        return kiriview::ImageZoomMode::Manual;
    }
    return kiriview::ImageZoomMode::Fit;
}

ImageViewportFitMode viewportFitMode(kiriview::ImageZoomMode fitMode)
{
    switch (fitMode) {
    case kiriview::ImageZoomMode::Fit:
        return ImageViewportFitMode::Contain;
    case kiriview::ImageZoomMode::FitWidth:
        return ImageViewportFitMode::FitWidth;
    case kiriview::ImageZoomMode::FitHeight:
        return ImageViewportFitMode::FitHeight;
    case kiriview::ImageZoomMode::Manual:
        return ImageViewportFitMode::Manual;
    }
    return ImageViewportFitMode::Contain;
}

qreal axisScanPosition(qreal position, qreal step, qreal maximum, bool forward)
{
    const qreal current = std::clamp(position, 0.0, maximum);
    if (maximum <= positionEpsilon || step <= positionEpsilon) {
        return current;
    }
    if (forward) {
        if (current >= maximum - positionEpsilon) {
            return current;
        }
        return std::min(maximum, std::floor(current / step) * step + step);
    }
    if (current <= positionEpsilon) {
        return current;
    }
    return std::max<qreal>(0.0, (std::ceil(current / step) - 1.0) * step);
}

QPointF invalidPoint()
{
    return {
        std::numeric_limits<qreal>::quiet_NaN(),
        std::numeric_limits<qreal>::quiet_NaN(),
    };
}
}

namespace kiriview {
ImageDocumentRuntime::ImageDocumentRuntime(QObject* documentObject, ChangeCallback changeCallback,
    ImageDocumentRuntimeDependencyOverrides dependencies,
    FileDeletionFailedCallback fileDeletionFailedCallback,
    UnsupportedOpenedCollectionVideoEnteredCallback unsupportedOpenedCollectionVideoEnteredCallback,
    ContainerNavigationBoundaryReachedCallback containerNavigationBoundaryReachedCallback)
    : changeBatcher(ImageDocumentChangeBatcher::ChangeBatchCallback(
          [this](const std::vector<ImageDocumentChange>& changes) { publishChanges(changes); }))
    , state(changeBatcher)
    , changeCallback(std::move(changeCallback))
    , navigationSourceResolver(dependencies.navigationSourceResolver.has_value()
              ? std::move(*dependencies.navigationSourceResolver)
              : NavigationSourceResolver())
{
    runtimeGraph = std::make_unique<ImageDocumentRuntimeGraph>(documentObject, state,
        std::move(dependencies),
        ImageDocumentRuntimeGraphCallbacks {
            [this](const std::vector<ImageDocumentChange>& changes) { notify(changes); },
            [this](const ImageDocumentSourceLoadRequest& request) { loadSource(request); },
            [this](const QUrl& url) { return navigationSourceResolver.resolveExternalSource(url); },
            std::move(fileDeletionFailedCallback),
            std::move(unsupportedOpenedCollectionVideoEnteredCallback),
            std::move(containerNavigationBoundaryReachedCallback),
        });
}

ImageDocumentRuntime::~ImageDocumentRuntime() { shutdown(); }

QUrl ImageDocumentRuntime::sourceUrl() const { return state.sourceUrl(); }

ImageDocumentPageKind ImageDocumentRuntime::sourceKind() const { return state.sourceKind(); }

void ImageDocumentRuntime::setSourceUrl(const QUrl& sourceUrl)
{
    setSource(navigationSourceResolver.resolveExternalSource(sourceUrl));
}

void ImageDocumentRuntime::setSource(const ResolvedNavigationSource& source)
{
    loadSource(ImageDocumentSourceLoadRequest::fromExternalSource(source));
}

MediaEntrySourceVideoPlaybackDeviceResult
ImageDocumentRuntime::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl) const
{
    return runtimeGraph->loadOpenedCollectionVideoPlaybackDevice(openedCollectionScope, videoUrl);
}

ImageDocumentStatus ImageDocumentRuntime::status() const
{
    if (state.status() != ImageDocumentStatus::Ready) {
        return state.status();
    }
    return projectionActive(viewportProjection()) ? viewportProjection().status : state.status();
}

bool ImageDocumentRuntime::loading() const
{
    if (state.status() != ImageDocumentStatus::Ready) {
        return state.loading();
    }
    return projectionActive(viewportProjection()) ? viewportProjection().loading : state.loading();
}

QString ImageDocumentRuntime::presentationLifecycleToken() const
{
    return QStringLiteral("image-presentation:%1").arg(state.presentationLifecycleRevision());
}

QString ImageDocumentRuntime::errorString() const
{
    if (state.status() != ImageDocumentStatus::Ready) {
        return state.errorString();
    }
    return projectionActive(viewportProjection()) ? viewportProjection().errorString
                                                  : state.errorString();
}

const std::optional<ImageLoadFailure>& ImageDocumentRuntime::loadFailure() const
{
    if (state.status() != ImageDocumentStatus::Ready) {
        return state.loadFailure();
    }
    return projectionActive(viewportProjection()) ? viewportProjection().failure
                                                  : state.loadFailure();
}

QString ImageDocumentRuntime::windowTitleFileName() const { return state.windowTitleFileName(); }

QUrl ImageDocumentRuntime::displayedUrl() const
{
    return projectionActive(viewportProjection()) ? viewportProjection().displayedUrl
                                                  : state.displayedUrl();
}

bool ImageDocumentRuntime::completeAuthoritativeDisplayAvailable() const
{
    return projectionActive(viewportProjection()) && viewportProjection().correlated
        && viewportProjection().completeAuthoritativeDisplayAvailable;
}

OpenedCollectionScopeLocation ImageDocumentRuntime::displayedOpenedCollectionScope() const
{
    return state.displayedOpenedCollectionScope();
}

QSize ImageDocumentRuntime::imageSize() const
{
    const QSize primary = primaryImageSize();
    const QSize secondary = secondaryImageSize();
    return QSize(
        primary.width() + secondary.width(), std::max(primary.height(), secondary.height()));
}

QSize ImageDocumentRuntime::primaryImageSize() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().primaryImageSize
        : QSize();
}

QSize ImageDocumentRuntime::secondaryImageSize() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().secondaryImageSize
        : QSize();
}

bool ImageDocumentRuntime::viewportHorizontallyPannable() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        && viewportProjection().horizontallyPannable;
}

bool ImageDocumentRuntime::viewportVerticallyPannable() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        && viewportProjection().verticallyPannable;
}

bool ImageDocumentRuntime::viewportPannable() const
{
    return viewportHorizontallyPannable() || viewportVerticallyPannable();
}

qreal ImageDocumentRuntime::horizontalScrollPosition() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().horizontalScrollPosition
        : 0.0;
}

qreal ImageDocumentRuntime::horizontalScrollPageSize() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().horizontalScrollPageSize
        : 1.0;
}

qreal ImageDocumentRuntime::verticalScrollPosition() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().verticalScrollPosition
        : 0.0;
}

qreal ImageDocumentRuntime::verticalScrollPageSize() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().verticalScrollPageSize
        : 1.0;
}

bool ImageDocumentRuntime::submitHorizontalScrollPosition(qreal position)
{
    return projectionMatchesReadyImage(state, viewportProjection())
        && runtimeGraph->viewportIntegration().submitHorizontalScrollPosition(position);
}

bool ImageDocumentRuntime::submitVerticalScrollPosition(qreal position)
{
    return projectionMatchesReadyImage(state, viewportProjection())
        && runtimeGraph->viewportIntegration().submitVerticalScrollPosition(position);
}

bool ImageDocumentRuntime::zoomPercentKnown() const
{
    const ImageViewportIntegrationProjection& projection = viewportProjection();
    return projectionMatchesReadyImage(state, projection) && std::isfinite(projection.zoomPercent)
        && projection.zoomPercent > 0.0;
}

qreal ImageDocumentRuntime::zoomPercent() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        ? viewportProjection().zoomPercent
        : 0.0;
}

bool ImageDocumentRuntime::requestManualZoomPercentAtCenter(qreal zoomPercent)
{
    const QSizeF viewportSize = viewportProjection().viewportSize;
    return requestAnchoredManualZoom(clampedManualZoomPercent(zoomPercent),
        QPointF(viewportSize.width() / 2.0, viewportSize.height() / 2.0));
}

bool ImageDocumentRuntime::requestZoomByStep(qreal stepCount, QPointF viewportAnchorPoint)
{
    if (status() != ImageDocumentStatus::Ready || !pointIsFinite(viewportAnchorPoint)) {
        return false;
    }
    const QPointF anchorPoint = nearestImageViewportPoint(viewportAnchorPoint);
    return pointIsFinite(anchorPoint)
        && runtimeGraph->viewportIntegration().zoomBySteps(stepCount, anchorPoint);
}

bool ImageDocumentRuntime::requestZoomByStepAtCenter(qreal stepCount)
{
    const QSizeF viewportSize = viewportProjection().viewportSize;
    return requestZoomByStep(
        stepCount, QPointF(viewportSize.width() / 2.0, viewportSize.height() / 2.0));
}

bool ImageDocumentRuntime::requestViewportPinchUpdate(
    qreal scaleFactor, QPointF previousViewportCentroid, QPointF currentViewportCentroid)
{
    const ImageViewportIntegrationProjection& projection = viewportProjection();
    if (!projectionMatchesReadyImage(state, projection) || !std::isfinite(scaleFactor)
        || scaleFactor <= 0.0 || !pointIsFinite(previousViewportCentroid)
        || !pointIsFinite(currentViewportCentroid)) {
        return false;
    }

    std::optional<qreal> requestedZoomPercent;
    QPointF zoomAnchor;
    if (scaleFactor != 1.0) {
        const qreal currentZoomPercent = projection.zoomPercent;
        const qreal minimumZoomPercent = projection.minimumManualZoomPercent;
        const qreal maximumZoomPercent = projection.maximumManualZoomPercent;
        if (!std::isfinite(currentZoomPercent) || currentZoomPercent <= 0.0
            || !std::isfinite(minimumZoomPercent) || minimumZoomPercent <= 0.0
            || !std::isfinite(maximumZoomPercent) || maximumZoomPercent < minimumZoomPercent) {
            return false;
        }

        qreal targetZoomPercent = 0.0;
        if (scaleFactor <= minimumZoomPercent / currentZoomPercent) {
            targetZoomPercent = minimumZoomPercent;
        } else if (scaleFactor >= maximumZoomPercent / currentZoomPercent) {
            targetZoomPercent = maximumZoomPercent;
        } else {
            targetZoomPercent = currentZoomPercent * scaleFactor;
        }
        if (targetZoomPercent != currentZoomPercent) {
            zoomAnchor = nearestImageViewportPoint(previousViewportCentroid);
            if (!pointIsFinite(zoomAnchor)) {
                return false;
            }
            requestedZoomPercent = targetZoomPercent;
        }
    }

    [[maybe_unused]] auto batch = state.beginChangeBatch();
    if (requestedZoomPercent.has_value()
        && !requestAnchoredManualZoom(*requestedZoomPercent, zoomAnchor)) {
        return false;
    }

    const QPointF panDelta = previousViewportCentroid - currentViewportCentroid;
    if (!panDelta.isNull()) {
        static_cast<void>(requestViewportPanBy(panDelta));
    }
    return true;
}

bool ImageDocumentRuntime::requestToggleFitOrActualSize(QPointF viewportPoint)
{
    if (status() != ImageDocumentStatus::Ready || !pointIsFinite(viewportPoint)) {
        return false;
    }
    if (zoomMode() != ImageZoomMode::Fit) {
        setFitMode(ImageZoomMode::Fit);
        return true;
    }
    return requestAnchoredManualZoom(100.0, nearestImageViewportPoint(viewportPoint));
}

ImageZoomMode ImageDocumentRuntime::zoomMode() const
{
    return imageZoomMode(viewportProjection().fitMode);
}

ImageZoomMode ImageDocumentRuntime::fitModeSelection() const { return fitModeSelectionPreference; }

qreal ImageDocumentRuntime::maximumManualZoomPercent() const
{
    return viewportProjection().maximumManualZoomPercent;
}

qreal ImageDocumentRuntime::clampedManualZoomPercent(qreal zoomPercent) const
{
    const ImageViewportIntegrationProjection& projection = viewportProjection();
    if (!std::isfinite(zoomPercent)) {
        return projection.preferredManualZoomPercent;
    }
    if (projection.maximumManualZoomPercent < projection.minimumManualZoomPercent) {
        return zoomPercent;
    }
    return std::clamp(
        zoomPercent, projection.minimumManualZoomPercent, projection.maximumManualZoomPercent);
}

qreal ImageDocumentRuntime::steppedManualZoomPercent(qreal stepCount) const
{
    const ImageViewportIntegrationProjection& projection = viewportProjection();
    if (!std::isfinite(stepCount) || projection.manualZoomStepFactor <= 0.0) {
        return projection.preferredManualZoomPercent;
    }
    return clampedManualZoomPercent(
        projection.zoomPercent * std::pow(projection.manualZoomStepFactor, stepCount));
}

quint64 ImageDocumentRuntime::requestViewportPanBy(QPointF delta)
{
    return viewportPannable() && pointIsFinite(delta)
            && runtimeGraph->viewportIntegration().panBy(delta)
        ? 1
        : 0;
}

quint64 ImageDocumentRuntime::requestViewportPanToInitialScanPosition()
{
    return viewportPannable() && submitContentPosition(scanBoundaryPosition(false)) ? 1 : 0;
}

quint64 ImageDocumentRuntime::requestViewportPanToFinalScanPosition()
{
    return viewportPannable() && submitContentPosition(scanBoundaryPosition(true)) ? 1 : 0;
}

quint64 ImageDocumentRuntime::requestViewportScanForward()
{
    return viewportPannable() && submitContentPosition(scanPosition(true)) ? 1 : 0;
}

quint64 ImageDocumentRuntime::requestViewportScanBackward()
{
    return viewportPannable() && submitContentPosition(scanPosition(false)) ? 1 : 0;
}

void ImageDocumentRuntime::requestNextViewportTargetAnchorAtEnd()
{
    runtimeGraph->requestNextViewportTargetAnchorAtEnd();
}

int ImageDocumentRuntime::currentPageNumber() const
{
    return runtimeGraph->navigationController().currentPageNumber();
}

int ImageDocumentRuntime::currentLastPageNumber() const
{
    return runtimeGraph->spreadController().currentLastPageNumber();
}

int ImageDocumentRuntime::pageCount() const
{
    return runtimeGraph->navigationController().pageCount();
}

ImageDocumentPageNavigationSnapshot ImageDocumentRuntime::pageNavigationSnapshot() const
{
    return runtimeGraph->navigationController().pageNavigationSnapshot();
}

const ImageDocumentPageCandidateListSnapshot&
ImageDocumentRuntime::confirmedPageCandidateSnapshot() const
{
    return runtimeGraph->navigationController().confirmedPageCandidateSnapshot();
}

ImageDocumentPageActiveNavigationSnapshot ImageDocumentRuntime::activeNavigationSnapshot() const
{
    return runtimeGraph->spreadController().activeNavigationSnapshot();
}

bool ImageDocumentRuntime::containerNavigationAvailable() const
{
    return state.containerNavigationAvailable();
}

bool ImageDocumentRuntime::ordinaryDirectMediaScopeActive() const
{
    return !displayedUrl().isEmpty() && state.displayedOpenedCollectionScope().isEmpty();
}

bool ImageDocumentRuntime::openedCollectionScopeActive() const
{
    return !state.selectedOpenedCollectionScope().isEmpty();
}

bool ImageDocumentRuntime::fileDeletionInProgress() const
{
    return runtimeGraph->deletionController().inProgress();
}

bool ImageDocumentRuntime::twoPageModeEnabled() const
{
    return runtimeGraph->spreadController().twoPageModeEnabled();
}

void ImageDocumentRuntime::setTwoPageModeEnabled(bool enabled)
{
    const bool enabling = enabled && !runtimeGraph->spreadController().twoPageModeEnabled();
    runtimeGraph->spreadController().setTwoPageModeEnabled(enabled);
    if (enabling) {
        runtimeGraph->viewportIntegration().resetImageTransforms();
    }
}

bool ImageDocumentRuntime::twoPageModeAvailable() const
{
    return runtimeGraph->spreadController().twoPageModeAvailable();
}

bool ImageDocumentRuntime::rightToLeftReadingEnabled() const
{
    return runtimeGraph->spreadController().rightToLeftReadingEnabled();
}

void ImageDocumentRuntime::setRightToLeftReadingEnabled(bool enabled)
{
    runtimeGraph->spreadController().setRightToLeftReadingEnabled(enabled);
    runtimeGraph->viewportIntegration().setSpreadDirection(enabled
            ? ImageViewportSpreadDirection::RightToLeft
            : ImageViewportSpreadDirection::LeftToRight);
}

bool ImageDocumentRuntime::rightToLeftReadingAvailable() const
{
    return runtimeGraph->spreadController().rightToLeftReadingAvailable();
}

bool ImageDocumentRuntime::secondaryPageVisible() const
{
    return projectionMatchesReadyImage(state, viewportProjection())
        && viewportProjection().secondaryVisible;
}

QPointF ImageDocumentRuntime::nearestImageViewportPoint(QPointF viewportPoint) const
{
    if (!projectionMatchesReadyImage(state, viewportProjection())) {
        return invalidPoint();
    }
    const QRectF contentRect = viewportProjection().contentRect;
    if (contentRect.isEmpty() || !pointIsFinite(viewportPoint)) {
        return invalidPoint();
    }
    const qreal maximumX = std::nextafter(contentRect.right(), contentRect.left());
    const qreal maximumY = std::nextafter(contentRect.bottom(), contentRect.top());
    const QPointF clampedPoint(std::clamp(viewportPoint.x(), contentRect.left(), maximumX),
        std::clamp(viewportPoint.y(), contentRect.top(), maximumY));
    ImageViewportCoordinateInput input;
    input.setSourceSpace(ImageViewportCoordinateSpace::Item);
    input.setTargetSpace(ImageViewportCoordinateSpace::DisplayedSpread);
    input.setPoint(clampedPoint);
    return runtimeGraph->viewportIntegration().mapPoint(input).isValid() ? clampedPoint
                                                                         : invalidPoint();
}

bool ImageDocumentRuntime::unsupportedOpenedCollectionVideo() const
{
    return state.unsupportedOpenedCollectionVideo();
}

std::optional<DisplayedPredecodeImage> ImageDocumentRuntime::primaryDisplayedPredecodeImage() const
{
    return runtimeGraph->primaryDisplayedPredecodeImage();
}

ImageFirstDisplayDecodeContext ImageDocumentRuntime::firstDisplayDecodeContext() const
{
    return runtimeGraph->firstDisplayDecodeContext();
}

const EmbeddedMetadata& ImageDocumentRuntime::embeddedMetadata() const
{
    return state.embeddedMetadata();
}

void ImageDocumentRuntime::attachImageViewport(ImageViewport* viewport)
{
    runtimeGraph->viewportIntegration().attach(viewport);
}

void ImageDocumentRuntime::detachImageViewport(ImageViewport* viewport)
{
    runtimeGraph->viewportIntegration().detach(viewport);
}

const ImageViewportIntegrationProjection& ImageDocumentRuntime::viewportProjection() const
{
    return runtimeGraph->viewportIntegration().projection();
}

void ImageDocumentRuntime::notify(const std::vector<ImageDocumentChange>& changes)
{
    [[maybe_unused]] auto batch = state.beginChangeBatch();
    changeBatcher.notifyAll(changes);
    for (ImageDocumentChange change : changes) {
        runtimeGraph->spreadController().handleDocumentChange(change);
    }
}

void ImageDocumentRuntime::shutdown() { runtimeGraph->shutdownRuntime(); }

void ImageDocumentRuntime::openPreviousPage()
{
    runtimeGraph->navigationController().openAdjacentPage(NavigationDirection::Previous);
}

void ImageDocumentRuntime::openNextPage()
{
    runtimeGraph->navigationController().openAdjacentPage(NavigationDirection::Next);
}

void ImageDocumentRuntime::openPreviousSinglePage()
{
    runtimeGraph->navigationController().openImageAtRelativePageOffset(-1);
}

void ImageDocumentRuntime::openNextSinglePage()
{
    runtimeGraph->navigationController().openImageAtRelativePageOffset(1);
}

void ImageDocumentRuntime::openImageAtPage(int pageNumber)
{
    runtimeGraph->navigationController().openImageAtPage(pageNumber);
}

void ImageDocumentRuntime::openPreviousContainer()
{
    runtimeGraph->navigationController().openAdjacentContainer(NavigationDirection::Previous);
}

void ImageDocumentRuntime::openNextContainer()
{
    runtimeGraph->navigationController().openAdjacentContainer(NavigationDirection::Next);
}

void ImageDocumentRuntime::deleteDisplayedFile(FileDeletionMode mode)
{
    runtimeGraph->deletionController().deleteDisplayedFile(mode);
}

void ImageDocumentRuntime::resetZoom()
{
    const ImageZoomMode previousSelection = fitModeSelectionPreference;
    fitModeSelectionPreference = ImageZoomMode::Fit;
    if (!runtimeGraph->viewportIntegration().resetView()) {
        fitModeSelectionPreference = previousSelection;
    }
}

void ImageDocumentRuntime::setFitMode(ImageZoomMode zoomMode)
{
    const ImageZoomMode previousSelection = fitModeSelectionPreference;
    if (zoomMode != ImageZoomMode::Manual) {
        fitModeSelectionPreference = zoomMode;
    }
    if (!runtimeGraph->viewportIntegration().setFitMode(viewportFitMode(zoomMode))) {
        fitModeSelectionPreference = previousSelection;
    }
}

void ImageDocumentRuntime::rotateClockwise()
{
    if (!runtimeGraph->spreadController().twoPageModeActive()) {
        runtimeGraph->viewportIntegration().rotateByQuarterTurns(1);
    }
}

void ImageDocumentRuntime::rotateCounterclockwise()
{
    if (!runtimeGraph->spreadController().twoPageModeActive()) {
        runtimeGraph->viewportIntegration().rotateByQuarterTurns(-1);
    }
}

void ImageDocumentRuntime::flipHorizontally()
{
    if (!runtimeGraph->spreadController().twoPageModeActive()) {
        runtimeGraph->viewportIntegration().toggleMirrorHorizontally();
    }
}

void ImageDocumentRuntime::flipVertically()
{
    if (!runtimeGraph->spreadController().twoPageModeActive()) {
        runtimeGraph->viewportIntegration().toggleMirrorVertically();
    }
}

QPointF ImageDocumentRuntime::scanPosition(bool forward) const
{
    const ImageViewportIntegrationProjection& projection = viewportProjection();
    const QPointF current = projection.contentPosition;
    const QPointF maximum = projection.maximumContentPosition;
    const bool rightToLeft = rightToLeftReadingEnabled() && rightToLeftReadingAvailable();
    if (maximum.x() > positionEpsilon) {
        const bool horizontalForward = rightToLeft ? !forward : forward;
        const qreal x
            = axisScanPosition(current.x(), projection.viewportSize.width() * scanStepViewportRatio,
                maximum.x(), horizontalForward);
        if (std::abs(x - current.x()) > positionEpsilon) {
            return { x, current.y() };
        }
    }
    if (maximum.y() > positionEpsilon) {
        const qreal y = axisScanPosition(current.y(),
            projection.viewportSize.height() * scanStepViewportRatio, maximum.y(), forward);
        if (std::abs(y - current.y()) > positionEpsilon) {
            const qreal rowStart
                = forward ? (rightToLeft ? maximum.x() : 0.0) : (rightToLeft ? 0.0 : maximum.x());
            return { rowStart, y };
        }
    }
    return current;
}

QPointF ImageDocumentRuntime::scanBoundaryPosition(bool final) const
{
    const QPointF maximum = viewportProjection().maximumContentPosition;
    const bool rightToLeft = rightToLeftReadingEnabled() && rightToLeftReadingAvailable();
    return final ? QPointF(rightToLeft ? 0.0 : maximum.x(), maximum.y())
                 : QPointF(rightToLeft ? maximum.x() : 0.0, 0.0);
}

bool ImageDocumentRuntime::submitContentPosition(QPointF position)
{
    return pointIsFinite(position) && position != viewportProjection().contentPosition
        && runtimeGraph->viewportIntegration().setContentPosition(position);
}

bool ImageDocumentRuntime::requestAnchoredManualZoom(qreal zoomPercent, QPointF viewportAnchorPoint)
{
    return status() == ImageDocumentStatus::Ready && pointIsFinite(viewportAnchorPoint)
        && runtimeGraph->viewportIntegration().setPreferredManualZoomPercent(
            clampedManualZoomPercent(zoomPercent), viewportAnchorPoint);
}

void ImageDocumentRuntime::loadSource(const ImageDocumentSourceLoadRequest& request)
{
    runtimeGraph->dispatchSourceLoadPlan(ImageOpenWorkflow::sourceLoadPlan(
        ImageDocumentSourceLoadSnapshot {
            state.sourceUrl(),
            state.displayedOpenedCollectionScope(),
            rightToLeftReadingEnabled(),
        },
        request));
}

void ImageDocumentRuntime::publishChanges(const std::vector<ImageDocumentChange>& changes)
{
    invokeIfSet(changeCallback, changes);
}
}
