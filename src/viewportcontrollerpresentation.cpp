#include "imageviewportvalidation_p.h"
#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollergeometryhelpers_p.h"

#include <algorithm>
#include <cmath>

namespace {
QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

bool applyContentPosition(ViewportControllerPort& viewport,
    ImageViewportInternal::PresentationState& presentation, QPointF requestedPosition)
{
    const PresentationGeometry::State geometry = controllerGeometryState(viewport, presentation);
    if (PresentationGeometry::contentRect(geometry).isEmpty() || geometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = PresentationGeometry::contentPosition(geometry);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition && presentation.contentPosition == nextPosition) {
        return false;
    }

    presentation.contentPosition = nextPosition;
    return true;
}

bool applyContentPositionForGeometry(ImageViewportInternal::PresentationState& presentation,
    const PresentationGeometry::State& geometry, QPointF requestedPosition)
{
    if (PresentationGeometry::contentRect(geometry).isEmpty() || geometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = PresentationGeometry::contentPosition(geometry);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition && presentation.contentPosition == nextPosition) {
        return false;
    }

    presentation.contentPosition = nextPosition;
    return true;
}

bool clampPresentationContentPositionToBounds(
    ViewportControllerPort& viewport, ImageViewportInternal::PresentationState& presentation)
{
    const PresentationGeometry::State currentGeometry
        = controllerGeometryState(viewport, presentation);
    if (PresentationGeometry::contentRect(currentGeometry).isEmpty()
        || currentGeometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF clampedPosition = PresentationGeometry::contentPosition(currentGeometry);
    if (presentation.contentPosition == clampedPosition) {
        return false;
    }

    presentation.contentPosition = clampedPosition;
    return true;
}

void preserveAnchoredContentPosition(ViewportControllerPort& viewport,
    ImageViewportInternal::PresentationState& presentation,
    const PresentationGeometry::State& previousGeometry, QPointF anchor)
{
    const CoordinateResult anchoredSpreadPoint
        = PresentationGeometry::itemToSpread(previousGeometry, anchor.x(), anchor.y());
    if (!anchoredSpreadPoint.isValid()) {
        clampPresentationContentPositionToBounds(viewport, presentation);
        return;
    }

    const PresentationGeometry::State nextGeometry
        = controllerGeometryState(viewport, presentation);
    presentation.contentPosition = PresentationGeometry::contentPositionForAnchoredSpreadPoint(
        nextGeometry, QPointF(anchoredSpreadPoint.x(), anchoredSpreadPoint.y()), anchor);
}

ImageViewportInternal::ViewportChangeSet presentationChanges(
    ViewportControllerPort& viewport, bool affectsGeometry)
{
    ImageViewportInternal::ViewportChangeSet changes;
    changes.presentation = true;
    changes.displayRevision = true;
    changes.geometryState
        = affectsGeometry && viewport.hasReadyDisplay() && !viewport.itemBounds().isEmpty();
    changes.scheduleUpdate = true;
    return changes;
}

ViewportCommandResult acceptedPresentationCommand(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet changes = {})
{
    return ImageViewportInternal::CommandOutcome::accepted(viewport, changes);
}

ViewportCommandResult invalidPresentationCommand(ViewportControllerPort& viewport)
{
    return ImageViewportInternal::CommandOutcome::invalid(viewport);
}

ViewportCommandResult preservedPresentationCommand(ImageViewport::CommandOutcome outcome)
{
    ViewportCommandResult result;
    result.outcome = outcome;
    return result;
}
}

ImageViewportInternal::ViewportChangeSet ViewportController::applyPresentationTransition(
    const ControllerTransitionPolicy& policy, QPointF previousContentPosition,
    double previousZoomPercent)
{
    auto& presentation = state.engine.presentationState();
    ImageViewportInternal::ViewportChangeSet changes;
    auto markChanged = [&]() { mergeChanges(changes, presentationChanges(viewport, true)); };

    if (policy.magnificationPolicy == PageSetTransitionPolicy::ZoomTransition::ResetToContain) {
        if (presentation.fitMode != ImageViewport::FitMode::Contain
            || presentation.manualZoom != 1.0) {
            presentation.fitMode = ImageViewport::FitMode::Contain;
            presentation.manualZoom = 1.0;
            markChanged();
        }
    }
    if (policy.explicitFitMode && presentation.fitMode != *policy.explicitFitMode) {
        presentation.fitMode = *policy.explicitFitMode;
        markChanged();
    }
    if (policy.magnificationPolicy == PageSetTransitionPolicy::ZoomTransition::Preserve
        && presentation.fitMode == ImageViewport::FitMode::Manual
        && ImageViewportInternal::isFinitePositive(previousZoomPercent)) {
        const double previousManualZoom = previousZoomPercent / 100.0;
        if (presentation.manualZoom != previousManualZoom) {
            presentation.manualZoom = previousManualZoom;
            markChanged();
        }
    }
    if (policy.rotationTransition == PageSetTransitionPolicy::RotationTransition::Reset
        && presentation.rotationDegrees != 0) {
        presentation.rotationDegrees = 0;
        markChanged();
    }
    if (policy.mirrorTransition == PageSetTransitionPolicy::MirrorTransition::Reset
        && (presentation.mirrorHorizontally || presentation.mirrorVertically)) {
        presentation.mirrorHorizontally = false;
        presentation.mirrorVertically = false;
        markChanged();
    }
    if (policy.explicitSpreadDirection
        && presentation.spreadDirection != *policy.explicitSpreadDirection) {
        presentation.spreadDirection = *policy.explicitSpreadDirection;
        markChanged();
    }
    if (policy.explicitPageGap && presentation.pageGap != *policy.explicitPageGap) {
        presentation.pageGap = *policy.explicitPageGap;
        markChanged();
    }

    if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::ScanStart) {
        if (applyContentPositionForGeometry(
                presentation, acceptedGeometryState(viewport, presentation), {})) {
            markChanged();
        }
    } else if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::ScanEnd) {
        const PresentationGeometry::State geometry = acceptedGeometryState(viewport, presentation);
        if (applyContentPositionForGeometry(
                presentation, geometry, PresentationGeometry::maximumContentPosition(geometry))) {
            markChanged();
        }
    } else if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::Clamp) {
        if (applyContentPositionForGeometry(presentation,
                acceptedGeometryState(viewport, presentation), previousContentPosition)) {
            markChanged();
        }
    }

    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::setSmoothing(bool smoothing)
{
    if (state.engine.presentationState().smoothing == smoothing) {
        return {};
    }

    state.engine.presentationState().smoothing = smoothing;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMipmap(bool mipmap)
{
    if (state.engine.presentationState().mipmap == mipmap) {
        return {};
    }

    state.engine.presentationState().mipmap = mipmap;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorHorizontally(bool enabled)
{
    if (state.engine.presentationState().mirrorHorizontally == enabled) {
        return {};
    }

    state.engine.presentationState().mirrorHorizontally = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorVertically(bool enabled)
{
    if (state.engine.presentationState().mirrorVertically == enabled) {
        return {};
    }

    state.engine.presentationState().mirrorVertically = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundMode(
    ImageViewport::BackgroundMode mode)
{
    if (!ImageViewportInternal::isValidBackgroundMode(mode)
        || state.engine.presentationState().backgroundMode == mode) {
        return {};
    }

    state.engine.presentationState().backgroundMode = mode;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundColor(const QColor& color)
{
    if (state.engine.presentationState().backgroundColor == color) {
        return {};
    }

    state.engine.presentationState().backgroundColor = color;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setQualityPreference(
    ImageViewport::QualityPreference preference)
{
    if (!ImageViewportInternal::isValidQualityPreference(preference)
        || state.engine.presentationState().qualityPreference == preference) {
        return {};
    }

    state.engine.presentationState().qualityPreference = preference;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setExactnessPreference(
    ImageViewport::ExactnessPreference preference)
{
    if (!ImageViewportInternal::isValidExactnessPreference(preference)
        || state.engine.presentationState().exactnessPreference == preference) {
        return {};
    }

    state.engine.presentationState().exactnessPreference = preference;
    return presentationChanges(viewport, false);
}

ViewportCommandResult ViewportController::setSpreadDirection(
    ImageViewport::SpreadDirection direction)
{
    if (!ImageViewportInternal::isValidSpreadDirection(direction)) {
        return preservedPresentationCommand(ImageViewport::CommandOutcome::Invalid);
    }
    if (state.engine.presentationState().spreadDirection == direction) {
        return preservedPresentationCommand(ImageViewport::CommandOutcome::Accepted);
    }

    state.engine.presentationState().spreadDirection = direction;
    clampPresentationContentPositionToBounds(viewport, state.engine.presentationState());
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setPageGap(double gap)
{
    if (!std::isfinite(gap) || gap < 0.0) {
        return preservedPresentationCommand(ImageViewport::CommandOutcome::Invalid);
    }
    if (state.engine.presentationState().pageGap == gap) {
        return preservedPresentationCommand(ImageViewport::CommandOutcome::Accepted);
    }

    state.engine.presentationState().pageGap = gap;
    clampPresentationContentPositionToBounds(viewport, state.engine.presentationState());
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setFitMode(ImageViewport::FitMode mode, QPointF anchor)
{
    if (!ImageViewportInternal::isValidFitMode(mode)
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.engine.presentationState().fitMode == mode) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().fitMode = mode;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setZoomPercent(
    double percent, QPointF anchor, double devicePixelRatio)
{
    if (!ImageViewportInternal::isFinitePositive(percent)
        || percent > maximumManualZoomPercent(devicePixelRatio)
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const double manualZoom = percent / 100.0;
    if (state.engine.presentationState().fitMode == ImageViewport::FitMode::Manual
        && state.engine.presentationState().manualZoom == manualZoom) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().fitMode = ImageViewport::FitMode::Manual;
    state.engine.presentationState().manualZoom = manualZoom;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::zoomByStep(
    int stepCount, QPointF anchor, double devicePixelRatio)
{
    return setZoomPercent(
        steppedManualZoomPercent(stepCount, devicePixelRatio), anchor, devicePixelRatio);
}

ViewportCommandResult ViewportController::panBy(QPointF delta)
{
    if (!ImageViewportInternal::isFinitePoint(delta)) {
        return invalidPresentationCommand(viewport);
    }
    if (delta.isNull()) {
        return acceptedPresentationCommand(viewport);
    }

    if (!applyContentPosition(viewport, state.engine.presentationState(),
            controllerContentPosition(viewport, state.engine.presentationState()) + delta)) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToStart()
{
    if (!applyContentPosition(viewport, state.engine.presentationState(), {})) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToEnd()
{
    if (!applyContentPosition(viewport, state.engine.presentationState(),
            controllerMaximumContentPosition(viewport, state.engine.presentationState()))) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::scanNext()
{
    const QPointF maximum
        = controllerMaximumContentPosition(viewport, state.engine.presentationState());
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
    const QPointF maximum
        = controllerMaximumContentPosition(viewport, state.engine.presentationState());
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
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().rotationDegrees
        = (state.engine.presentationState().rotationDegrees + 90) % 360;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::rotateCounterClockwise(QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().rotationDegrees
        = (state.engine.presentationState().rotationDegrees + 270) % 360;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setMirrorHorizontally(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.engine.presentationState().mirrorHorizontally == enabled) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().mirrorHorizontally = enabled;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setMirrorVertically(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.engine.presentationState().mirrorVertically == enabled) {
        return acceptedPresentationCommand(viewport);
    }

    const PresentationGeometry::State previousGeometry
        = controllerGeometryState(viewport, state.engine.presentationState());
    state.engine.presentationState().mirrorVertically = enabled;
    preserveAnchoredContentPosition(
        viewport, state.engine.presentationState(), previousGeometry, anchor);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::resetView()
{
    const bool changed = state.engine.presentationState().fitMode != ImageViewport::FitMode::Contain
        || state.engine.presentationState().manualZoom != 1.0
        || state.engine.presentationState().contentPosition.x() != 0.0
        || state.engine.presentationState().contentPosition.y() != 0.0;
    state.engine.presentationState().fitMode = ImageViewport::FitMode::Contain;
    state.engine.presentationState().manualZoom = 1.0;
    state.engine.presentationState().contentPosition = {};

    return acceptedPresentationCommand(viewport,
        changed ? presentationChanges(viewport, true)
                : ImageViewportInternal::ViewportChangeSet {});
}
