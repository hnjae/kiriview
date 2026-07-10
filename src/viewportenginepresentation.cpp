#include "viewportengine_p.h"

#include "imageviewportvalidation_p.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
using ImageViewportInternal::PresentationState;
using ImageViewportInternal::ViewportChangeSet;

bool presentationStatesEqual(const PresentationState& left, const PresentationState& right)
{
    return left.fitMode == right.fitMode && left.spreadDirection == right.spreadDirection
        && left.backgroundMode == right.backgroundMode
        && left.qualityPreference == right.qualityPreference
        && left.exactnessPreference == right.exactnessPreference
        && left.backgroundColor == right.backgroundColor && left.manualZoom == right.manualZoom
        && left.pageGap == right.pageGap && left.rotationDegrees == right.rotationDegrees
        && left.contentPosition == right.contentPosition && left.smoothing == right.smoothing
        && left.mipmap == right.mipmap && left.mirrorHorizontally == right.mirrorHorizontally
        && left.mirrorVertically == right.mirrorVertically;
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

bool applyContentPosition(PresentationState& presentation,
    const PresentationGeometry::State& geometry, QPointF requestedPosition)
{
    if (PresentationGeometry::contentRect(geometry).isEmpty() || geometry.itemBounds.isEmpty()) {
        return false;
    }
    const QPointF nextPosition = clampedPoint(
        requestedPosition, {}, PresentationGeometry::maximumContentPosition(geometry));
    if (presentation.contentPosition == nextPosition) {
        return false;
    }
    presentation.contentPosition = nextPosition;
    return true;
}

void preserveAnchoredContentPosition(ViewportEngine& engine, PresentationState& presentation,
    const ViewportEngine::GeometryInput& geometryInput,
    const PresentationGeometry::State& previousGeometry, QPointF anchor)
{
    const CoordinateResult anchoredSpreadPoint
        = PresentationGeometry::itemToSpread(previousGeometry, anchor.x(), anchor.y());
    const PresentationGeometry::State nextGeometry
        = engine.geometryState(geometryInput, presentation);
    if (!anchoredSpreadPoint.isValid()) {
        presentation.contentPosition = PresentationGeometry::contentPosition(nextGeometry);
        return;
    }
    presentation.contentPosition = PresentationGeometry::contentPositionForAnchoredSpreadPoint(
        nextGeometry, QPointF(anchoredSpreadPoint.x(), anchoredSpreadPoint.y()), anchor);
}

QSizeF orientedSpreadSize(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(state);
    const int rotation = ((state.rotationDegrees % 360) + 360) % 360;
    return rotation == 90 || rotation == 270 ? QSizeF(spreadSize.height(), spreadSize.width())
                                             : spreadSize;
}

double effectiveZoomPercent(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !spreadSize.isValid() || spreadSize.width() <= 0.0
        || spreadSize.height() <= 0.0) {
        return state.manualZoom * 100.0;
    }
    return content.width() / spreadSize.width() * state.devicePixelRatio * 100.0;
}

double steppedZoomPercent(int stepCount, const PresentationGeometry::State& geometry)
{
    const double minimum = std::numeric_limits<double>::denorm_min() > 0.0
        ? std::numeric_limits<double>::denorm_min()
        : std::numeric_limits<double>::min();
    const double maximum = ImageViewportDisplayLimits::maximumManualZoomPercent();
    const double base = std::clamp(effectiveZoomPercent(geometry), minimum, maximum);
    const double targetLog = std::log(base) + static_cast<double>(stepCount) * std::log(1.25);
    if (!std::isfinite(targetLog) || targetLog >= std::log(maximum)) {
        return maximum;
    }
    if (targetLog <= std::log(minimum)) {
        return minimum;
    }
    return std::clamp(std::exp(targetLog), minimum, maximum);
}

bool commandHasOperation(const ImageViewportPresentationCommand& command)
{
    return command.resetView() || command.hasFitMode() || command.hasManualZoomPercent()
        || command.hasZoomStepDelta() || command.hasContentPosition() || command.hasPanDelta()
        || command.hasScanDirection() || command.hasRotationDegrees()
        || command.hasMirrorHorizontally() || command.hasMirrorVertically()
        || command.hasSpreadDirection() || command.hasPageGap() || command.hasBackgroundMode()
        || command.hasBackgroundColor() || command.hasSmoothing() || command.hasMipmap()
        || command.hasLooping() || command.hasQualityPreference()
        || command.hasExactnessPreference();
}

bool commandValid(const ViewportEngine::PresentationCommandInput& input)
{
    const ImageViewportPresentationCommand& command = input.command;
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
    const double maximumZoom = ImageViewportDisplayLimits::maximumManualZoomPercent();

    return commandHasOperation(command) && !resetConflicts && geometryPositioningOperations <= 1
        && ImageViewportInternal::isFinitePoint(input.anchor)
        && std::isfinite(input.geometry.devicePixelRatio) && input.geometry.devicePixelRatio > 0.0
        && (!command.hasFitMode() || ImageViewportInternal::isValidFitMode(command.fitMode()))
        && (!command.hasManualZoomPercent()
            || (ImageViewportInternal::isFinitePositive(command.manualZoomPercent())
                && command.manualZoomPercent() <= maximumZoom))
        && (!command.hasContentPosition()
            || ImageViewportInternal::isFinitePoint(command.contentPosition()))
        && (!command.hasPanDelta() || ImageViewportInternal::isFinitePoint(command.panDelta()))
        && (!command.hasScanDirection()
            || ImageViewportInternal::isValidScanDirection(command.scanDirection()))
        && (!command.hasRotationDegrees() || validRotation(command.rotationDegrees()))
        && (!command.hasSpreadDirection()
            || ImageViewportInternal::isValidSpreadDirection(command.spreadDirection()))
        && (!command.hasPageGap() || (std::isfinite(command.pageGap()) && command.pageGap() >= 0.0))
        && (!command.hasBackgroundMode()
            || ImageViewportInternal::isValidBackgroundMode(command.backgroundMode()))
        && (!command.hasQualityPreference()
            || ImageViewportInternal::isValidQualityPreference(command.qualityPreference()))
        && (!command.hasExactnessPreference()
            || ImageViewportInternal::isValidExactnessPreference(command.exactnessPreference()));
}

ViewportChangeSet presentationChanges(
    bool presentationChanged, bool affectsGeometry, bool readyDisplay, const QRectF& itemBounds)
{
    ViewportChangeSet changes;
    if (!presentationChanged) {
        return changes;
    }
    changes.displayRevision = true;
    changes.geometryState = affectsGeometry && readyDisplay && !itemBounds.isEmpty();
    changes.scheduleUpdate = true;
    return changes;
}
}

ViewportEngine::PresentationCommandResult ViewportEngine::applyPresentationCommand(
    const PresentationCommandInput& input)
{
    PresentationCommandResult result;
    if (!commandValid(input)) {
        result.command = rejectInvalidCommand();
        return result;
    }

    const PresentationState previousPresentation = m_presentationState;
    const bool previousLooping = m_requestState.looping;
    PresentationState next = previousPresentation;
    bool nextLooping = previousLooping;
    bool affectsGeometry = false;
    const auto geometry = [&] { return geometryState(input.geometry, next); };
    const auto applyAnchored = [&](auto mutation) {
        const PresentationGeometry::State previousGeometry = geometry();
        mutation();
        preserveAnchoredContentPosition(
            *this, next, input.geometry, previousGeometry, input.anchor);
        affectsGeometry = true;
    };
    const ImageViewportPresentationCommand& command = input.command;

    if (command.resetView()) {
        next.fitMode = ImageViewport::FitMode::Contain;
        next.manualZoom = 1.0;
        next.contentPosition = {};
        next.rotationDegrees = 0;
        next.mirrorHorizontally = false;
        next.mirrorVertically = false;
        affectsGeometry = true;
    }
    if (command.hasFitMode() && next.fitMode != command.fitMode()) {
        applyAnchored([&] { next.fitMode = command.fitMode(); });
    }
    if (command.hasManualZoomPercent()) {
        const double manualZoom = command.manualZoomPercent() / 100.0;
        if (next.fitMode != ImageViewport::FitMode::Manual || next.manualZoom != manualZoom) {
            applyAnchored([&] {
                next.fitMode = ImageViewport::FitMode::Manual;
                next.manualZoom = manualZoom;
            });
        }
    }
    if (command.hasZoomStepDelta()) {
        const double manualZoom = steppedZoomPercent(command.zoomStepDelta(), geometry()) / 100.0;
        if (next.fitMode != ImageViewport::FitMode::Manual || next.manualZoom != manualZoom) {
            applyAnchored([&] {
                next.fitMode = ImageViewport::FitMode::Manual;
                next.manualZoom = manualZoom;
            });
        }
    }
    if (command.hasContentPosition()) {
        affectsGeometry
            = applyContentPosition(next, geometry(), command.contentPosition()) || affectsGeometry;
    }
    if (command.hasPanDelta()) {
        const QPointF current = PresentationGeometry::contentPosition(geometry());
        affectsGeometry = applyContentPosition(next, geometry(), current + command.panDelta())
            || affectsGeometry;
    }
    if (command.hasScanDirection()) {
        QPointF requested;
        const PresentationGeometry::State currentGeometry = geometry();
        const QPointF maximum = PresentationGeometry::maximumContentPosition(currentGeometry);
        switch (command.scanDirection()) {
        case ImageViewport::ScanDirection::Start:
            requested = {};
            break;
        case ImageViewport::ScanDirection::End:
            requested = maximum;
            break;
        case ImageViewport::ScanDirection::Next:
            requested = PresentationGeometry::contentPosition(currentGeometry)
                + (maximum.y() > 0.0
                        ? QPointF(0.0, std::max(1.0, input.geometry.itemBounds.height() * 0.9))
                        : QPointF(std::max(1.0, input.geometry.itemBounds.width() * 0.9), 0.0));
            break;
        case ImageViewport::ScanDirection::Previous:
            requested = PresentationGeometry::contentPosition(currentGeometry)
                + (maximum.y() > 0.0
                        ? QPointF(0.0, -std::max(1.0, input.geometry.itemBounds.height() * 0.9))
                        : QPointF(-std::max(1.0, input.geometry.itemBounds.width() * 0.9), 0.0));
            break;
        }
        affectsGeometry = applyContentPosition(next, currentGeometry, requested) || affectsGeometry;
    }
    if (command.hasRotationDegrees() && next.rotationDegrees != command.rotationDegrees()) {
        applyAnchored([&] { next.rotationDegrees = command.rotationDegrees(); });
    }
    if (command.hasMirrorHorizontally()
        && next.mirrorHorizontally != command.mirrorHorizontally()) {
        applyAnchored([&] { next.mirrorHorizontally = command.mirrorHorizontally(); });
    }
    if (command.hasMirrorVertically() && next.mirrorVertically != command.mirrorVertically()) {
        applyAnchored([&] { next.mirrorVertically = command.mirrorVertically(); });
    }
    if (command.hasSpreadDirection() && next.spreadDirection != command.spreadDirection()) {
        next.spreadDirection = command.spreadDirection();
        next.contentPosition = PresentationGeometry::contentPosition(geometry());
        affectsGeometry = true;
    }
    if (command.hasPageGap() && next.pageGap != command.pageGap()) {
        next.pageGap = command.pageGap();
        next.contentPosition = PresentationGeometry::contentPosition(geometry());
        affectsGeometry = true;
    }
    if (command.hasBackgroundMode()) {
        next.backgroundMode = command.backgroundMode();
    }
    if (command.hasBackgroundColor()) {
        next.backgroundColor = command.backgroundColor();
    }
    if (command.hasSmoothing()) {
        next.smoothing = command.smoothing();
    }
    if (command.hasMipmap()) {
        next.mipmap = command.mipmap();
    }
    if (command.hasLooping()) {
        nextLooping = command.looping();
    }
    if (command.hasQualityPreference()) {
        next.qualityPreference = command.qualityPreference();
    }
    if (command.hasExactnessPreference()) {
        next.exactnessPreference = command.exactnessPreference();
    }

    const bool presentationChanged = !presentationStatesEqual(previousPresentation, next);
    const bool changed = presentationChanged || previousLooping != nextLooping;
    if (!changed) {
        result.command = acceptedPreservingCommandDiagnostics();
        return result;
    }
    m_presentationState = next;
    m_requestState.looping = nextLooping;
    result.command = accepted();
    result.changes = presentationChanges(
        presentationChanged, affectsGeometry, input.readyDisplay, input.geometry.itemBounds);
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::applyPresentationTargetTransition(
    const PresentationTargetTransitionInput& input)
{
    const PresentationState previousPresentation = m_presentationState;
    PresentationState next = previousPresentation;
    if (input.zoomTransition
        == PresentationTargetTransitionPolicy::ZoomTransition::ResetToContain) {
        next.fitMode = ImageViewport::FitMode::Contain;
        next.manualZoom = 1.0;
    }
    if (input.explicitFitMode) {
        next.fitMode = *input.explicitFitMode;
    }
    if (input.zoomTransition == PresentationTargetTransitionPolicy::ZoomTransition::Preserve
        && next.fitMode == ImageViewport::FitMode::Manual
        && ImageViewportInternal::isFinitePositive(input.previousZoomPercent)) {
        next.manualZoom = input.previousZoomPercent / 100.0;
    }
    if (input.rotationTransition == PresentationTargetTransitionPolicy::RotationTransition::Reset) {
        next.rotationDegrees = 0;
    }
    if (input.mirrorTransition == PresentationTargetTransitionPolicy::MirrorTransition::Reset) {
        next.mirrorHorizontally = false;
        next.mirrorVertically = false;
    }
    if (input.explicitSpreadDirection) {
        next.spreadDirection = *input.explicitSpreadDirection;
    }
    if (input.explicitPageGap) {
        next.pageGap = *input.explicitPageGap;
    }

    const PresentationGeometry::State acceptedGeometry
        = geometryState(input.acceptedGeometry, next);
    switch (input.contentPositionTransition) {
    case PresentationTargetTransitionPolicy::ContentPositionTransition::ScanStart:
        applyContentPosition(next, acceptedGeometry, {});
        break;
    case PresentationTargetTransitionPolicy::ContentPositionTransition::ScanEnd:
        applyContentPosition(
            next, acceptedGeometry, PresentationGeometry::maximumContentPosition(acceptedGeometry));
        break;
    case PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp:
        applyContentPosition(next, acceptedGeometry, input.previousContentPosition);
        break;
    case PresentationTargetTransitionPolicy::ContentPositionTransition::Preserve:
        break;
    }

    const bool changed = !presentationStatesEqual(previousPresentation, next);
    if (!changed) {
        return {};
    }
    m_presentationState = next;
    return presentationChanges(true, true, input.readyDisplay, input.acceptedGeometry.itemBounds);
}
