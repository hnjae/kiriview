#include "viewportcontrollerhelpers_p.h"

ImageViewportInternal::ViewportChangeSet ViewportController::setSmoothing(bool smoothing)
{
    if (state.presentation.smoothing == smoothing) {
        return {};
    }

    state.presentation.smoothing = smoothing;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMipmap(bool mipmap)
{
    if (state.presentation.mipmap == mipmap) {
        return {};
    }

    state.presentation.mipmap = mipmap;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorHorizontally(bool enabled)
{
    if (state.presentation.mirrorHorizontally == enabled) {
        return {};
    }

    state.presentation.mirrorHorizontally = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorVertically(bool enabled)
{
    if (state.presentation.mirrorVertically == enabled) {
        return {};
    }

    state.presentation.mirrorVertically = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundMode(
    ImageViewport::BackgroundMode mode)
{
    if (!ImageViewportInternal::isValidBackgroundMode(mode)
        || state.presentation.backgroundMode == mode) {
        return {};
    }

    state.presentation.backgroundMode = mode;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundColor(const QColor& color)
{
    if (state.presentation.backgroundColor == color) {
        return {};
    }

    state.presentation.backgroundColor = color;
    return presentationChanges(viewport, false);
}

ViewportCommandResult ViewportController::setSpreadDirection(
    ImageViewport::SpreadDirection direction)
{
    if (!ImageViewportInternal::isValidSpreadDirection(direction)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.spreadDirection == direction) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.spreadDirection = direction;
    clampPresentationContentPositionToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setPageGap(double gap)
{
    if (!std::isfinite(gap) || gap < 0.0) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.pageGap == gap) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.pageGap = gap;
    clampPresentationContentPositionToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setFitMode(ImageViewport::FitMode mode, QPointF anchor)
{
    if (!ImageViewportInternal::isValidFitMode(mode)
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.fitMode == mode) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.fitMode = mode;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setZoomPercent(double percent, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePositive(percent)
        || percent > ImageViewportDisplayLimits::maximumManualZoomPercent()
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const double manualZoom = percent / 100.0;
    if (state.presentation.fitMode == ImageViewport::FitMode::Manual
        && state.presentation.manualZoom == manualZoom) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.fitMode = ImageViewport::FitMode::Manual;
    state.presentation.manualZoom = manualZoom;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panBy(QPointF delta)
{
    if (!ImageViewportInternal::isFinitePoint(delta)) {
        return invalidPresentationCommand(viewport);
    }
    if (delta.isNull()) {
        return acceptedPresentationCommand(viewport);
    }

    if (!applyContentPosition(viewport, state.presentation,
            controllerContentPosition(viewport, state.presentation) + delta)) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToStart()
{
    if (!applyContentPosition(viewport, state.presentation, {})) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToEnd()
{
    if (!applyContentPosition(viewport, state.presentation,
            controllerMaximumContentPosition(viewport, state.presentation))) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::scanNext()
{
    const QPointF maximum = controllerMaximumContentPosition(viewport, state.presentation);
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, std::max(1.0, viewport.itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(std::max(1.0, viewport.itemBounds().width() * 0.9), 0.0));
    }
    return acceptedPresentationCommand(viewport);
}

ViewportCommandResult ViewportController::scanPrevious()
{
    const QPointF maximum = controllerMaximumContentPosition(viewport, state.presentation);
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, -std::max(1.0, viewport.itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(-std::max(1.0, viewport.itemBounds().width() * 0.9), 0.0));
    }
    return acceptedPresentationCommand(viewport);
}

ViewportCommandResult ViewportController::rotateClockwise(QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.rotationDegrees = (state.presentation.rotationDegrees + 90) % 360;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::rotateCounterClockwise(QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.rotationDegrees = (state.presentation.rotationDegrees + 270) % 360;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setMirrorHorizontally(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.mirrorHorizontally == enabled) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.mirrorHorizontally = enabled;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setMirrorVertically(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.mirrorVertically == enabled) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.presentation);
    state.presentation.mirrorVertically = enabled;
    preserveAnchoredContentPosition(viewport, state.presentation, previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::resetView()
{
    const bool changed = state.presentation.fitMode != ImageViewport::FitMode::Contain
        || state.presentation.manualZoom != 1.0 || state.presentation.contentPosition.x() != 0.0
        || state.presentation.contentPosition.y() != 0.0;
    state.presentation.fitMode = ImageViewport::FitMode::Contain;
    state.presentation.manualZoom = 1.0;
    state.presentation.contentPosition = {};

    return acceptedPresentationCommand(viewport,
        changed ? presentationChanges(viewport, true)
                : ImageViewportInternal::ViewportChangeSet {});
}
