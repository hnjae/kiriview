#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <algorithm>
#include <cmath>

using namespace ImageViewportInternal;

namespace {

bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

QSizeF secondaryImageSize(const ImageViewportPrivate& viewport)
{
    return viewport.secondaryLogicalSize();
}

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    const ImageViewportInternal::PresentationState& presentation
        = viewport.controller.presentationState();
    return {
        viewport.hasReadyDisplay(),
        viewport.itemBounds(),
        viewport.currentImageSize(),
        secondaryImageSize(viewport),
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.fillMode,
        presentation.horizontalAlignment,
        presentation.verticalAlignment,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.zoom,
        effectiveDevicePixelRatio(viewport),
        presentation.pan,
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
    state.primaryImageSize = imageSize;
    state.secondaryImageSize = {};
    return state;
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

QPointF contentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    const QPointF maximum(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
    return clampedPoint(QPointF(-contentRect.x(), -contentRect.y()), {}, maximum);
}

QPointF maximumContentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    return QPointF(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
}

QSizeF orientedSpreadSize(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(state);
    const int rotation = ((state.rotationDegrees % 360) + 360) % 360;
    if (rotation == 90 || rotation == 270) {
        return QSizeF(spreadSize.height(), spreadSize.width());
    }
    return spreadSize;
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

QRectF ImageViewportPrivate::visibleSpreadRect() const
{
    return PresentationGeometry::visibleSpreadRect(geometryState(*this));
}

QRectF ImageViewportPrivate::primaryPageRect() const
{
    return PresentationGeometry::primaryPageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::secondaryPageRect() const
{
    return PresentationGeometry::secondaryPageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::primaryItemRect() const
{
    return PresentationGeometry::pageItemRect(geometryState(*this), PageRole::Primary);
}

QRectF ImageViewportPrivate::secondaryItemRect() const
{
    return PresentationGeometry::pageItemRect(geometryState(*this), PageRole::Secondary);
}

QRectF ImageViewportPrivate::visiblePrimaryPageRect() const
{
    return PresentationGeometry::visiblePageRect(geometryState(*this), PageRole::Primary);
}

QRectF ImageViewportPrivate::visibleSecondaryPageRect() const
{
    return PresentationGeometry::visiblePageRect(geometryState(*this), PageRole::Secondary);
}

QSizeF ImageViewportPrivate::contentSize() const { return contentRect().size(); }

QPointF ImageViewportPrivate::contentPosition() const
{
    return contentPositionForRect(contentRect(), itemBounds());
}

QPointF ImageViewportPrivate::maximumContentPosition() const
{
    return maximumContentPositionForRect(contentRect(), itemBounds());
}

bool ImageViewportPrivate::horizontalPannable() const { return maximumContentPosition().x() > 0.0; }

bool ImageViewportPrivate::verticalPannable() const { return maximumContentPosition().y() > 0.0; }

QRectF ImageViewportPrivate::contentRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::contentRect(geometryStateForImageSize(*this, imageSize));
}

QRectF ImageViewportPrivate::visibleImageRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::visibleImageRect(geometryStateForImageSize(*this, imageSize));
}

ImageViewportPrivate::FitMode ImageViewportPrivate::fitMode() const
{
    return controller.presentationState().fitMode;
}

void ImageViewportPrivate::setFitModeProperty(FitMode mode) { setFitMode(mode, {}); }

double ImageViewportPrivate::zoomPercent() const
{
    const PresentationGeometry::State state = geometryState(*this);
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !isPositiveSize(spreadSize)) {
        return controller.presentationState().zoom * 100.0;
    }

    return content.width() / spreadSize.width() * effectiveDevicePixelRatio(*this) * 100.0;
}

void ImageViewportPrivate::setZoomPercentProperty(double percent) { setZoomPercent(percent, {}); }

int ImageViewportPrivate::rotationDegrees() const
{
    return controller.presentationState().rotationDegrees;
}

bool ImageViewportPrivate::smoothing() const { return controller.presentationState().smoothing; }

void ImageViewportPrivate::setSmoothing(bool smoothing)
{
    applyControllerChanges(controller.setSmoothing(smoothing));
}

bool ImageViewportPrivate::mipmap() const { return controller.presentationState().mipmap; }

void ImageViewportPrivate::setMipmap(bool mipmap)
{
    applyControllerChanges(controller.setMipmap(mipmap));
}

bool ImageViewportPrivate::mirrorHorizontally() const
{
    return controller.presentationState().mirrorHorizontally;
}

void ImageViewportPrivate::setMirrorHorizontally(bool mirror)
{
    applyControllerChanges(controller.setMirrorHorizontally(mirror));
}

bool ImageViewportPrivate::mirrorVertically() const
{
    return controller.presentationState().mirrorVertically;
}

void ImageViewportPrivate::setMirrorVertically(bool mirror)
{
    applyControllerChanges(controller.setMirrorVertically(mirror));
}

ImageViewportPrivate::BackgroundMode ImageViewportPrivate::backgroundMode() const
{
    return controller.presentationState().backgroundMode;
}

void ImageViewportPrivate::setBackgroundMode(BackgroundMode mode)
{
    applyControllerChanges(controller.setBackgroundMode(mode));
}

QColor ImageViewportPrivate::backgroundColor() const
{
    return controller.presentationState().backgroundColor;
}

void ImageViewportPrivate::setBackgroundColor(const QColor& color)
{
    applyControllerChanges(controller.setBackgroundColor(color));
}

bool ImageViewportPrivate::looping() const { return controller.looping(); }

void ImageViewportPrivate::setLooping(bool looping)
{
    applyControllerChanges(controller.setLooping(looping));
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setSpreadDirection(
    SpreadDirection direction)
{
    const ViewportCommandResult result = controller.setSpreadDirection(direction);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageGap(double gap)
{
    const ViewportCommandResult result = controller.setPageGap(gap);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setFitMode(FitMode mode, QPointF anchor)
{
    const ViewportCommandResult result = controller.setFitMode(mode, anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setZoomPercent(
    double percent, QPointF anchor)
{
    const ViewportCommandResult result = controller.setZoomPercent(percent, anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panBy(QPointF delta)
{
    const ViewportCommandResult result = controller.panBy(delta);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToStart()
{
    const ViewportCommandResult result = controller.panToStart();
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToEnd()
{
    const ViewportCommandResult result = controller.panToEnd();
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanNext()
{
    const ViewportCommandResult result = controller.scanNext();
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanPrevious()
{
    const ViewportCommandResult result = controller.scanPrevious();
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateClockwise(QPointF anchor)
{
    const ViewportCommandResult result = controller.rotateClockwise(anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateCounterClockwise(QPointF anchor)
{
    const ViewportCommandResult result = controller.rotateCounterClockwise(anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorHorizontally(
    bool enabled, QPointF anchor)
{
    const ViewportCommandResult result = controller.setMirrorHorizontally(enabled, anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorVertically(
    bool enabled, QPointF anchor)
{
    const ViewportCommandResult result = controller.setMirrorVertically(enabled, anchor);
    applyControllerChanges(result.changes);
    return result.outcome;
}

CoordinateResult ImageViewportPrivate::itemToSpread(double x, double y) const
{
    return PresentationGeometry::itemToSpread(geometryState(*this), x, y);
}

CoordinateResult ImageViewportPrivate::spreadToItem(double x, double y) const
{
    return PresentationGeometry::spreadToItem(geometryState(*this), x, y);
}

CoordinateResult ImageViewportPrivate::itemToPage(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return invalidCoordinateResult();
    }

    return PresentationGeometry::itemToPage(geometryState(*this), role, x, y);
}

CoordinateResult ImageViewportPrivate::pageToItem(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return invalidCoordinateResult();
    }

    return PresentationGeometry::pageToItem(geometryState(*this), role, x, y);
}

bool ImageViewportPrivate::containsVisibleSpreadPoint(double x, double y) const
{
    return PresentationGeometry::containsVisibleSpreadPoint(geometryState(*this), x, y);
}

bool ImageViewportPrivate::containsVisiblePagePoint(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return false;
    }

    return PresentationGeometry::containsVisiblePagePoint(geometryState(*this), role, x, y);
}

CoordinateResult ImageViewportPrivate::itemToImage(double x, double y) const
{
    return PresentationGeometry::itemToImage(geometryState(*this), x, y);
}

CoordinateResult ImageViewportPrivate::imageToItem(double x, double y) const
{
    return PresentationGeometry::imageToItem(geometryState(*this), x, y);
}

bool ImageViewportPrivate::containsVisibleImagePoint(double x, double y) const
{
    return PresentationGeometry::containsVisibleImagePoint(geometryState(*this), x, y);
}

ImageViewportRange ImageViewportPrivate::invalidRange() { return {}; }

CoordinateResult ImageViewportPrivate::invalidCoordinateResult()
{
    return PresentationGeometry::invalidCoordinateResult();
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
