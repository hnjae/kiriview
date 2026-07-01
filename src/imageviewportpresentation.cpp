#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <algorithm>

using namespace ImageViewportInternal;

namespace {

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    return {
        viewport.hasReadyDisplay(),
        viewport.itemBounds(),
        viewport.currentImageSize(),
        viewport.presentation.fillMode,
        viewport.presentation.horizontalAlignment,
        viewport.presentation.verticalAlignment,
        viewport.presentation.mirrorHorizontally,
        viewport.presentation.mirrorVertically,
        viewport.presentation.zoom,
        viewport.presentation.pan,
    };
}

PresentationGeometry::State geometryStateForItemBounds(
    const ImageViewportPrivate& viewport, const QRectF& bounds)
{
    PresentationGeometry::State state = geometryState(viewport);
    state.itemBounds = bounds;
    return state;
}

PresentationGeometry::State geometryStateForImageSize(
    const ImageViewportPrivate& viewport, QSizeF imageSize)
{
    PresentationGeometry::State state = geometryState(viewport);
    state.hasReadyDisplay = !imageSize.isEmpty();
    state.imageSize = imageSize;
    return state;
}

}

QRectF ImageViewportPrivate::contentRect() const
{
    return PresentationGeometry::contentRect(geometryState(*this));
}

QRectF ImageViewportPrivate::visibleImageRect() const
{
    return PresentationGeometry::visibleImageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::visibleSpreadRect() const { return visibleImageRect(); }

QRectF ImageViewportPrivate::primaryPageRect() const
{
    const QSizeF imageSize = currentImageSize();
    if (imageSize.isEmpty()) {
        return {};
    }

    return QRectF(QPointF(0.0, 0.0), imageSize);
}

QRectF ImageViewportPrivate::secondaryPageRect() const { return {}; }

QRectF ImageViewportPrivate::primaryItemRect() const { return contentRect(); }

QRectF ImageViewportPrivate::secondaryItemRect() const { return {}; }

QRectF ImageViewportPrivate::visiblePrimaryPageRect() const { return visibleImageRect(); }

QRectF ImageViewportPrivate::visibleSecondaryPageRect() const { return {}; }

QSizeF ImageViewportPrivate::contentSize() const { return contentRect().size(); }

QPointF ImageViewportPrivate::contentPosition() const { return presentation.pan; }

QPointF ImageViewportPrivate::maximumContentPosition() const { return {}; }

bool ImageViewportPrivate::horizontalPannable() const { return false; }

bool ImageViewportPrivate::verticalPannable() const { return false; }

QRectF ImageViewportPrivate::contentRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::contentRect(geometryStateForImageSize(*this, imageSize));
}

QRectF ImageViewportPrivate::visibleImageRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::visibleImageRect(geometryStateForImageSize(*this, imageSize));
}

ImageViewportPrivate::FitMode ImageViewportPrivate::fitMode() const { return presentation.fitMode; }

void ImageViewportPrivate::setFitModeProperty(FitMode mode) { setFitMode(mode, {}); }

double ImageViewportPrivate::zoomPercent() const { return presentation.zoom * 100.0; }

void ImageViewportPrivate::setZoomPercentProperty(double percent) { setZoomPercent(percent, {}); }

int ImageViewportPrivate::rotationDegrees() const { return presentation.rotationDegrees; }

ImageViewportPrivate::FillMode ImageViewportPrivate::fillMode() const
{
    return presentation.fillMode;
}

void ImageViewportPrivate::setFillMode(FillMode mode)
{
    if (!isValidFillMode(mode) || presentation.fillMode == mode) {
        return;
    }

    presentation.fillMode = mode;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::HorizontalAlignment ImageViewportPrivate::horizontalAlignment() const
{
    return presentation.horizontalAlignment;
}

void ImageViewportPrivate::setHorizontalAlignment(HorizontalAlignment alignment)
{
    if (!isValidHorizontalAlignment(alignment) || presentation.horizontalAlignment == alignment) {
        return;
    }

    presentation.horizontalAlignment = alignment;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::VerticalAlignment ImageViewportPrivate::verticalAlignment() const
{
    return presentation.verticalAlignment;
}

void ImageViewportPrivate::setVerticalAlignment(VerticalAlignment alignment)
{
    if (!isValidVerticalAlignment(alignment) || presentation.verticalAlignment == alignment) {
        return;
    }

    presentation.verticalAlignment = alignment;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::smoothing() const { return presentation.smoothing; }

void ImageViewportPrivate::setSmoothing(bool smoothing)
{
    if (presentation.smoothing == smoothing) {
        return;
    }

    presentation.smoothing = smoothing;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mipmap() const { return presentation.mipmap; }

void ImageViewportPrivate::setMipmap(bool mipmap)
{
    if (presentation.mipmap == mipmap) {
        return;
    }

    presentation.mipmap = mipmap;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mirrorHorizontally() const { return presentation.mirrorHorizontally; }

void ImageViewportPrivate::setMirrorHorizontally(bool mirror)
{
    if (presentation.mirrorHorizontally == mirror) {
        return;
    }

    presentation.mirrorHorizontally = mirror;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::mirrorVertically() const { return presentation.mirrorVertically; }

void ImageViewportPrivate::setMirrorVertically(bool mirror)
{
    if (presentation.mirrorVertically == mirror) {
        return;
    }

    presentation.mirrorVertically = mirror;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::BackgroundMode ImageViewportPrivate::backgroundMode() const
{
    return presentation.backgroundMode;
}

void ImageViewportPrivate::setBackgroundMode(BackgroundMode mode)
{
    if (!isValidBackgroundMode(mode) || presentation.backgroundMode == mode) {
        return;
    }

    presentation.backgroundMode = mode;
    notifyPresentationChanged(false);
}

QColor ImageViewportPrivate::backgroundColor() const { return presentation.backgroundColor; }

void ImageViewportPrivate::setBackgroundColor(const QColor& color)
{
    if (presentation.backgroundColor == color) {
        return;
    }

    presentation.backgroundColor = color;
    notifyPresentationChanged(false);
}

double ImageViewportPrivate::zoom() const { return presentation.zoom; }

void ImageViewportPrivate::setZoom(double zoom)
{
    if (!isFinitePositive(zoom) || presentation.zoom == zoom) {
        return;
    }

    presentation.zoom = zoom;
    notifyPresentationChanged(true);
}

QPointF ImageViewportPrivate::pan() const { return presentation.pan; }

void ImageViewportPrivate::setPan(QPointF pan)
{
    const bool unchanged = presentation.pan.x() == pan.x() && presentation.pan.y() == pan.y();
    if (!isFinitePoint(pan) || unchanged) {
        return;
    }

    presentation.pan = pan;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::looping() const { return controller.looping(); }

void ImageViewportPrivate::setLooping(bool looping)
{
    applyControllerChanges(controller.setLooping(looping));
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setSpreadDirection(
    SpreadDirection direction)
{
    if (!isValidSpreadDirection(direction)) {
        return CommandOutcome::Invalid;
    }
    if (presentation.spreadDirection == direction) {
        return CommandOutcome::Accepted;
    }

    presentation.spreadDirection = direction;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageGap(double gap)
{
    if (!std::isfinite(gap) || gap < 0.0) {
        return CommandOutcome::Invalid;
    }
    if (presentation.pageGap == gap) {
        return CommandOutcome::Accepted;
    }

    presentation.pageGap = gap;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setFitMode(FitMode mode, QPointF anchor)
{
    if (!isValidFitMode(mode) || !isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }
    if (presentation.fitMode == mode) {
        return CommandOutcome::Accepted;
    }

    presentation.fitMode = mode;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setZoomPercent(
    double percent, QPointF anchor)
{
    if (!isFinitePositive(percent) || !isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    const double zoom = percent / 100.0;
    if (presentation.fitMode == FitMode::Manual && presentation.zoom == zoom) {
        return CommandOutcome::Accepted;
    }

    presentation.fitMode = FitMode::Manual;
    presentation.zoom = zoom;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panBy(QPointF delta)
{
    if (!isFinitePoint(delta)) {
        return CommandOutcome::Invalid;
    }
    if (delta.isNull()) {
        return CommandOutcome::Accepted;
    }

    presentation.pan += delta;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToStart()
{
    if (presentation.pan.isNull()) {
        return CommandOutcome::Accepted;
    }

    presentation.pan = {};
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToEnd()
{
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanNext()
{
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanPrevious()
{
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateClockwise(QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    presentation.rotationDegrees = (presentation.rotationDegrees + 90) % 360;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateCounterClockwise(QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    presentation.rotationDegrees = (presentation.rotationDegrees + 270) % 360;
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorHorizontally(
    bool enabled, QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    setMirrorHorizontally(enabled);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorVertically(
    bool enabled, QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    setMirrorVertically(enabled);
    return CommandOutcome::Accepted;
}

QVariantMap ImageViewportPrivate::itemToSpread(double x, double y) const
{
    return itemToImage(x, y);
}

QVariantMap ImageViewportPrivate::spreadToItem(double x, double y) const
{
    return imageToItem(x, y);
}

QVariantMap ImageViewportPrivate::itemToPage(PageRole role, double x, double y) const
{
    if (role != PageRole::Primary) {
        return invalidCoordinateResult();
    }

    return itemToImage(x, y);
}

QVariantMap ImageViewportPrivate::pageToItem(PageRole role, double x, double y) const
{
    if (role != PageRole::Primary) {
        return invalidCoordinateResult();
    }

    return imageToItem(x, y);
}

bool ImageViewportPrivate::containsVisibleSpreadPoint(double x, double y) const
{
    return containsVisibleImagePoint(x, y);
}

bool ImageViewportPrivate::containsVisiblePagePoint(PageRole role, double x, double y) const
{
    return role == PageRole::Primary && containsVisibleImagePoint(x, y);
}

QVariantMap ImageViewportPrivate::itemToImage(double x, double y) const
{
    return PresentationGeometry::itemToImage(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::imageToItem(double x, double y) const
{
    return PresentationGeometry::imageToItem(geometryState(*this), x, y);
}

bool ImageViewportPrivate::containsVisibleImagePoint(double x, double y) const
{
    return PresentationGeometry::containsVisibleImagePoint(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::invalidRange()
{
    return {
        { QStringLiteral("minimum"), -1 },
        { QStringLiteral("maximum"), -1 },
    };
}

QVariantMap ImageViewportPrivate::invalidCoordinateResult()
{
    return PresentationGeometry::invalidCoordinateResult();
}

void ImageViewportPrivate::notifyPresentationChanged(bool affectsGeometry)
{
    incrementDisplayRevision();
    emit q->presentationChanged();
    if (affectsGeometry && hasReadyDisplay() && !itemBounds().isEmpty()) {
        emit q->geometryStateChanged();
    }
    update();
}

QRectF ImageViewportPrivate::currentContentRect() const
{
    return PresentationGeometry::contentRect(geometryState(*this));
}

QRectF ImageViewportPrivate::itemBounds() const
{
    if (width() <= 0.0 || height() <= 0.0) {
        return {};
    }

    return QRectF(0.0, 0.0, width(), height());
}

QRectF ImageViewportPrivate::contentRectForItemBounds(const QRectF& bounds) const
{
    return PresentationGeometry::contentRect(geometryStateForItemBounds(*this, bounds));
}

QRectF ImageViewportPrivate::visibleImageRectForItemBounds(const QRectF& bounds) const
{
    return PresentationGeometry::visibleImageRect(geometryStateForItemBounds(*this, bounds));
}

QSizeF ImageViewportPrivate::currentImageSize() const
{
    if (!hasReadyDisplay()) {
        return {};
    }

    return controller.displayState().displayedImageSize;
}
