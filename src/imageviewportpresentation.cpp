#include "imageviewport_p.h"
#include "imageviewportvalidation_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollercommandcontract_p.h"

#include <QtQuick/QQuickWindow>

#include <cmath>
#include <optional>

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

double ImageViewportPrivate::minimumManualZoomPercent() const
{
    return controller.minimumManualZoomPercent();
}

double ImageViewportPrivate::maximumManualZoomPercent() const
{
    return controller.maximumManualZoomPercent(effectiveDevicePixelRatio(*this));
}

double ImageViewportPrivate::manualZoomStepFactor() const
{
    return controller.manualZoomStepFactor();
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
    const ViewportCommandResult result
        = controller.setZoomPercent(percent, anchor, effectiveDevicePixelRatio(*this));
    applyControllerChanges(result.changes);
    return result.outcome;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::zoomByStep(int stepCount, QPointF anchor)
{
    const ViewportCommandResult result
        = controller.zoomByStep(stepCount, anchor, effectiveDevicePixelRatio(*this));
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

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPresentation(
    ImageViewportPresentationCommand command)
{
    const bool resetConflicts = command.resetView()
        && (command.hasFitMode() || command.hasManualZoomPercent() || command.hasZoomStepDelta()
            || command.hasContentPosition() || command.hasPanDelta() || command.hasScanDirection()
            || command.hasRotationDegrees() || command.hasMirrorHorizontally()
            || command.hasMirrorVertically());
    const int geometryPositioningOperations = (command.hasManualZoomPercent() ? 1 : 0)
        + (command.hasZoomStepDelta() ? 1 : 0) + (command.hasContentPosition() ? 1 : 0)
        + (command.hasPanDelta() ? 1 : 0) + (command.hasScanDirection() ? 1 : 0);
    const auto validRotation = [](int degrees) {
        return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
    };
    const bool invalid = resetConflicts || geometryPositioningOperations > 1
        || (command.hasFitMode() && !ImageViewportInternal::isValidFitMode(command.fitMode()))
        || (command.hasManualZoomPercent()
            && (!ImageViewportInternal::isFinitePositive(command.manualZoomPercent())
                || command.manualZoomPercent()
                    > controller.maximumManualZoomPercent(effectiveDevicePixelRatio(*this))))
        || (command.hasContentPosition()
            && !ImageViewportInternal::isFinitePoint(command.contentPosition()))
        || (command.hasPanDelta() && !ImageViewportInternal::isFinitePoint(command.panDelta()))
        || (command.hasScanDirection()
            && !ImageViewportInternal::isValidScanDirection(command.scanDirection()))
        || (command.hasRotationDegrees() && !validRotation(command.rotationDegrees()))
        || (command.hasSpreadDirection()
            && !ImageViewportInternal::isValidSpreadDirection(command.spreadDirection()))
        || (command.hasPageGap() && (!std::isfinite(command.pageGap()) || command.pageGap() < 0.0))
        || (command.hasBackgroundMode()
            && !ImageViewportInternal::isValidBackgroundMode(command.backgroundMode()))
        || (command.hasQualityPreference()
            && !ImageViewportInternal::isValidQualityPreference(command.qualityPreference()))
        || (command.hasExactnessPreference()
            && !ImageViewportInternal::isValidExactnessPreference(command.exactnessPreference()));

    if (invalid) {
        const ViewportCommandResult result = controller.rejectInvalidCommand();
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    const QPointF anchor = itemCenter(*this);
    auto accepted = [](CommandOutcome outcome) { return outcome == CommandOutcome::Accepted; };

    if (command.resetView() && !accepted(resetView())) {
        return CommandOutcome::Invalid;
    }
    if (command.hasFitMode() && !accepted(setFitMode(command.fitMode(), anchor))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasManualZoomPercent()
        && !accepted(setZoomPercent(command.manualZoomPercent(), anchor))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasZoomStepDelta() && !accepted(zoomByStep(command.zoomStepDelta(), anchor))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasContentPosition()
        && !accepted(panBy(command.contentPosition() - contentPosition()))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasPanDelta() && !accepted(panBy(command.panDelta()))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasScanDirection()) {
        switch (command.scanDirection()) {
        case ImageViewport::ScanDirection::Start:
            if (!accepted(panToStart())) {
                return CommandOutcome::Invalid;
            }
            break;
        case ImageViewport::ScanDirection::Previous:
            if (!accepted(scanPrevious())) {
                return CommandOutcome::Invalid;
            }
            break;
        case ImageViewport::ScanDirection::Next:
            if (!accepted(scanNext())) {
                return CommandOutcome::Invalid;
            }
            break;
        case ImageViewport::ScanDirection::End:
            if (!accepted(panToEnd())) {
                return CommandOutcome::Invalid;
            }
            break;
        }
    }
    if (command.hasRotationDegrees()) {
        const auto normalize = [](int degrees) { return ((degrees % 360) + 360) % 360; };
        while (normalize(rotationDegrees()) != command.rotationDegrees()) {
            if (!accepted(rotateClockwise(anchor))) {
                return CommandOutcome::Invalid;
            }
        }
    }
    if (command.hasMirrorHorizontally()
        && !accepted(setMirrorHorizontally(command.mirrorHorizontally(), anchor))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasMirrorVertically()
        && !accepted(setMirrorVertically(command.mirrorVertically(), anchor))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasSpreadDirection() && !accepted(setSpreadDirection(command.spreadDirection()))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasPageGap() && !accepted(setPageGap(command.pageGap()))) {
        return CommandOutcome::Invalid;
    }
    if (command.hasBackgroundMode()) {
        setBackgroundMode(command.backgroundMode());
    }
    if (command.hasBackgroundColor()) {
        setBackgroundColor(command.backgroundColor());
    }
    if (command.hasSmoothing()) {
        setSmoothing(command.smoothing());
    }
    if (command.hasMipmap()) {
        setMipmap(command.mipmap());
    }
    if (command.hasLooping()) {
        setLooping(command.looping());
    }
    if (command.hasQualityPreference()) {
        applyControllerChanges(controller.setQualityPreference(command.qualityPreference()));
    }
    if (command.hasExactnessPreference()) {
        applyControllerChanges(controller.setExactnessPreference(command.exactnessPreference()));
    }
    return CommandOutcome::Accepted;
}

namespace {

bool coordinateSpaceValid(ImageViewport::CoordinateSpace space)
{
    switch (space) {
    case ImageViewport::CoordinateSpace::Item:
    case ImageViewport::CoordinateSpace::Spread:
    case ImageViewport::CoordinateSpace::Page:
        return true;
    }
    return false;
}

bool coordinateUsesPage(ImageViewportCoordinateInput input)
{
    return input.sourceSpace() == ImageViewport::CoordinateSpace::Page
        || input.targetSpace() == ImageViewport::CoordinateSpace::Page;
}

std::optional<ImageViewport::PageRole> coordinatePageRole(ImageViewportCoordinateInput input)
{
    if (!coordinateUsesPage(input)) {
        return ImageViewport::PageRole::Primary;
    }
    if (!input.pageRole().canConvert<ImageViewport::PageRole>()) {
        return std::nullopt;
    }
    const ImageViewport::PageRole role = input.pageRole().value<ImageViewport::PageRole>();
    return ImageViewportInternal::isValidPageRole(role) ? std::optional(role) : std::nullopt;
}

ImageViewportCoordinateResult coordinateResultFor(
    ImageViewportCoordinateInput input, CoordinateResult result)
{
    return ImageViewportCoordinateResult(result.isValid(), QPointF(result.x(), result.y()),
        input.sourceSpace(), input.targetSpace(), input.pageRole());
}

ImageViewportCoordinateResult invalidCoordinateResultFor(ImageViewportCoordinateInput input)
{
    return ImageViewportCoordinateResult(
        false, QPointF(), input.sourceSpace(), input.targetSpace(), input.pageRole());
}

} // namespace

ImageViewportCoordinateResult ImageViewportPrivate::mapPoint(
    ImageViewportCoordinateInput input) const
{
    if (!coordinateSpaceValid(input.sourceSpace()) || !coordinateSpaceValid(input.targetSpace())
        || !ImageViewportInternal::isFinitePoint(input.point())) {
        return invalidCoordinateResultFor(input);
    }
    const std::optional<PageRole> role = coordinatePageRole(input);
    if (!role) {
        return invalidCoordinateResultFor(input);
    }
    if (input.sourceSpace() == input.targetSpace()) {
        return ImageViewportCoordinateResult(
            true, input.point(), input.sourceSpace(), input.targetSpace(), input.pageRole());
    }

    const QPointF point = input.point();
    CoordinateResult result;
    if (input.sourceSpace() == ImageViewport::CoordinateSpace::Item
        && input.targetSpace() == ImageViewport::CoordinateSpace::Spread) {
        result = itemToSpread(point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Spread
        && input.targetSpace() == ImageViewport::CoordinateSpace::Item) {
        result = spreadToItem(point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Item
        && input.targetSpace() == ImageViewport::CoordinateSpace::Page) {
        result = itemToPage(*role, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Page
        && input.targetSpace() == ImageViewport::CoordinateSpace::Item) {
        result = pageToItem(*role, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Spread
        && input.targetSpace() == ImageViewport::CoordinateSpace::Page) {
        const CoordinateResult itemPoint = spreadToItem(point.x(), point.y());
        result = itemPoint.isValid() ? itemToPage(*role, itemPoint.x(), itemPoint.y())
                                     : CoordinateResult {};
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Page
        && input.targetSpace() == ImageViewport::CoordinateSpace::Spread) {
        const CoordinateResult itemPoint = pageToItem(*role, point.x(), point.y());
        result = itemPoint.isValid() ? itemToSpread(itemPoint.x(), itemPoint.y())
                                     : CoordinateResult {};
    }

    return coordinateResultFor(input, result);
}

bool ImageViewportPrivate::containsPoint(ImageViewportCoordinateInput input) const
{
    if (!coordinateSpaceValid(input.sourceSpace())
        || !ImageViewportInternal::isFinitePoint(input.point())) {
        return false;
    }
    const std::optional<PageRole> role = coordinatePageRole(input);
    if (!role) {
        return false;
    }
    const QPointF point = input.point();
    switch (input.sourceSpace()) {
    case ImageViewport::CoordinateSpace::Item:
        return itemToSpread(point.x(), point.y()).isValid();
    case ImageViewport::CoordinateSpace::Spread:
        return containsVisibleSpreadPoint(point.x(), point.y());
    case ImageViewport::CoordinateSpace::Page:
        return containsVisiblePagePoint(*role, point.x(), point.y());
    }
    return false;
}

ImageViewportCoordinateResult ImageViewportPrivate::nearestVisiblePoint(
    ImageViewportCoordinateInput input) const
{
    if (!coordinateSpaceValid(input.sourceSpace()) || !coordinateSpaceValid(input.targetSpace())
        || !ImageViewportInternal::isFinitePoint(input.point())) {
        return invalidCoordinateResultFor(input);
    }
    const std::optional<PageRole> role = coordinatePageRole(input);
    if (!role) {
        return invalidCoordinateResultFor(input);
    }

    const QPointF point = input.point();
    CoordinateResult nearest;
    if (input.sourceSpace() == ImageViewport::CoordinateSpace::Spread) {
        nearest = nearestVisibleSpreadPoint(point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Page) {
        nearest = nearestVisiblePagePoint(*role, point.x(), point.y());
    } else {
        const CoordinateResult spreadPoint = itemToSpread(point.x(), point.y());
        nearest = spreadPoint.isValid()
            ? nearestVisibleSpreadPoint(spreadPoint.x(), spreadPoint.y())
            : CoordinateResult {};
    }
    if (!nearest.isValid()) {
        return invalidCoordinateResultFor(input);
    }
    ImageViewportCoordinateInput mappedInput = input;
    mappedInput.setSourceSpace(input.sourceSpace() == ImageViewport::CoordinateSpace::Item
            ? ImageViewport::CoordinateSpace::Spread
            : input.sourceSpace());
    mappedInput.setTargetSpace(input.targetSpace());
    mappedInput.setPoint(QPointF(nearest.x(), nearest.y()));
    if (mappedInput.sourceSpace() == input.targetSpace()) {
        return coordinateResultFor(input, nearest);
    }
    const ImageViewportCoordinateResult mapped = mapPoint(mappedInput);
    return ImageViewportCoordinateResult(mapped.isValid(), mapped.point(), input.sourceSpace(),
        input.targetSpace(), input.pageRole());
}

CoordinateResult ImageViewportPrivate::itemToSpread(double x, double y) const
{
    return PresentationGeometry::itemToSpread(geometryState(*this), x, y);
}

CoordinateResult ImageViewportPrivate::spreadToItem(double x, double y) const
{
    return PresentationGeometry::spreadToItem(geometryState(*this), x, y);
}

CoordinateResult ImageViewportPrivate::nearestVisibleSpreadPoint(double x, double y) const
{
    return PresentationGeometry::nearestVisibleSpreadPoint(geometryState(*this), x, y);
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

CoordinateResult ImageViewportPrivate::nearestVisiblePagePoint(
    PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return invalidCoordinateResult();
    }

    return PresentationGeometry::nearestVisiblePagePoint(geometryState(*this), role, x, y);
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
