#include "imageviewport_p.h"
#include "imageviewportvalidation_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollercommandcontract_p.h"

#include <QtQuick/QQuickWindow>

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

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    return viewport.controller.geometryState(effectiveDevicePixelRatio(viewport));
}

PresentationGeometry::State geometryStateForItemBounds(
    const ImageViewportPrivate& viewport, const QRectF& bounds)
{
    return viewport.controller.geometryStateForItemBounds(
        bounds, effectiveDevicePixelRatio(viewport));
}

QPointF itemCenter(const ImageViewportPrivate& viewport)
{
    return QPointF(viewport.width() / 2.0, viewport.height() / 2.0);
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

PageGeometry pageGeometryForRole(
    const PresentationGeometry::State& state, ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return PageGeometry(role, {}, {}, {}, false);
    }
    const QRectF pageRect = role == ImageViewport::PageRole::Secondary
        ? PresentationGeometry::secondaryPageRect(state)
        : PresentationGeometry::primaryPageRect(state);
    const bool available = !pageRect.isEmpty();
    if (!available) {
        return PageGeometry(role, {}, {}, {}, false);
    }
    return PageGeometry(role, pageRect, PresentationGeometry::pageItemRect(state, role),
        PresentationGeometry::visiblePageRect(state, role), true);
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

PageGeometry ImageViewportPrivate::primaryPageGeometry() const
{
    return pageGeometry(PageRole::Primary);
}

PageGeometry ImageViewportPrivate::secondaryPageGeometry() const
{
    return pageGeometry(PageRole::Secondary);
}

PageGeometry ImageViewportPrivate::pageGeometry(PageRole role) const
{
    return pageGeometryForRole(geometryState(*this), role);
}

QSizeF ImageViewportPrivate::contentSize() const
{
    return PresentationGeometry::contentSize(geometryState(*this));
}

QPointF ImageViewportPrivate::contentPosition() const
{
    return PresentationGeometry::contentPosition(geometryState(*this));
}

QPointF ImageViewportPrivate::maximumContentPosition() const
{
    return PresentationGeometry::maximumContentPosition(geometryState(*this));
}

bool ImageViewportPrivate::horizontalPannable() const
{
    return PresentationGeometry::horizontalPannable(geometryState(*this));
}

bool ImageViewportPrivate::verticalPannable() const
{
    return PresentationGeometry::verticalPannable(geometryState(*this));
}

ImageViewportPrivate::FitMode ImageViewportPrivate::fitMode() const
{
    return controller.presentationState().fitMode;
}

void ImageViewportPrivate::setFitModeProperty(FitMode mode)
{
    setFitMode(mode, itemCenter(*this));
}

double ImageViewportPrivate::zoomPercent() const
{
    const PresentationGeometry::State state = geometryState(*this);
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !isPositiveSize(spreadSize)) {
        return controller.presentationState().manualZoom * 100.0;
    }

    return content.width() / spreadSize.width() * effectiveDevicePixelRatio(*this) * 100.0;
}

void ImageViewportPrivate::setZoomPercentProperty(double percent)
{
    setZoomPercent(percent, itemCenter(*this));
}

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
    setMirrorHorizontally(mirror, itemCenter(*this));
}

bool ImageViewportPrivate::mirrorVertically() const
{
    return controller.presentationState().mirrorVertically;
}

void ImageViewportPrivate::setMirrorVertically(bool mirror)
{
    setMirrorVertically(mirror, itemCenter(*this));
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
    auto* const controllerAccess = &controller;
    const ViewportCommandResult result = controllerAccess->panBy(delta);
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToStart()
{
    auto* const controllerAccess = &controller;
    const ViewportCommandResult result = controllerAccess->panToStart();
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToEnd()
{
    auto* const controllerAccess = &controller;
    const ViewportCommandResult result = controllerAccess->panToEnd();
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
