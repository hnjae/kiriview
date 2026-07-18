#include "viewportengine_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineprojection_p.h"

#include "imageviewportlimits_p.h"
#include "imageviewportvalidation_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
using ImageViewportInternal::PresentationState;
using ImageViewportInternal::ViewportChangeSet;
using ImageViewportInternal::ViewportDisplayLimits;

bool presentationStatesEqual(const PresentationState& left, const PresentationState& right)
{
    return left.fitMode == right.fitMode && left.spreadDirection == right.spreadDirection
        && left.backgroundMode == right.backgroundMode
        && left.qualityPreference == right.qualityPreference
        && left.exactnessPreference == right.exactnessPreference
        && left.backgroundColor == right.backgroundColor && left.manualZoom == right.manualZoom
        && left.checkerboardLightColor == right.checkerboardLightColor
        && left.checkerboardDarkColor == right.checkerboardDarkColor
        && left.checkerboardCellSize == right.checkerboardCellSize && left.pageGap == right.pageGap
        && left.rotationDegrees == right.rotationDegrees
        && left.contentPosition == right.contentPosition && left.smoothing == right.smoothing
        && left.mipmap == right.mipmap && left.mirrorHorizontally == right.mirrorHorizontally
        && left.mirrorVertically == right.mirrorVertically;
}

bool targetPresentationProjectionsEqual(
    const PresentationState& left, const PresentationState& right)
{
    if (left.fitMode != right.fitMode || left.contentPosition != right.contentPosition
        || left.rotationDegrees != right.rotationDegrees
        || left.mirrorHorizontally != right.mirrorHorizontally
        || left.mirrorVertically != right.mirrorVertically
        || left.spreadDirection != right.spreadDirection || left.pageGap != right.pageGap
        || left.backgroundMode != right.backgroundMode || left.smoothing != right.smoothing
        || left.mipmap != right.mipmap || left.qualityPreference != right.qualityPreference
        || left.exactnessPreference != right.exactnessPreference) {
        return false;
    }
    if ((left.fitMode == ImageViewportFitMode::Manual
            || right.fitMode == ImageViewportFitMode::Manual)
        && left.manualZoom != right.manualZoom) {
        return false;
    }
    if (left.backgroundMode == ImageViewportBackgroundMode::SolidColor
        && left.backgroundColor != right.backgroundColor) {
        return false;
    }
    return left.backgroundMode != ImageViewportBackgroundMode::Checkerboard
        || (left.checkerboardLightColor == right.checkerboardLightColor
            && left.checkerboardDarkColor == right.checkerboardDarkColor
            && left.checkerboardCellSize == right.checkerboardCellSize);
}

bool backingPresentationEqual(const PresentationState& left, const PresentationState& right)
{
    if (left.backgroundMode != right.backgroundMode) {
        return false;
    }
    if (left.backgroundMode == ImageViewportBackgroundMode::SolidColor) {
        return left.backgroundColor == right.backgroundColor;
    }
    if (left.backgroundMode == ImageViewportBackgroundMode::Checkerboard) {
        return left.checkerboardLightColor == right.checkerboardLightColor
            && left.checkerboardDarkColor == right.checkerboardDarkColor
            && left.checkerboardCellSize == right.checkerboardCellSize;
    }
    return true;
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

void preserveAnchoredContentPosition(PresentationState& presentation,
    const ViewportEngineGeometryInput& geometryInput,
    const PresentationGeometry::State& previousGeometry, QPointF anchor)
{
    const CoordinateResult anchoredSpreadPoint
        = PresentationGeometry::itemToSpread(previousGeometry, anchor.x(), anchor.y());
    const PresentationGeometry::State nextGeometry
        = projectViewportGeometryState(geometryInput, presentation);
    if (!anchoredSpreadPoint.isValid()) {
        presentation.contentPosition = PresentationGeometry::contentPosition(nextGeometry);
        return;
    }
    presentation.contentPosition = PresentationGeometry::contentPositionForAnchoredSpreadPoint(
        nextGeometry, QPointF(anchoredSpreadPoint.x(), anchoredSpreadPoint.y()), anchor);
}

double steppedZoomPercent(int stepCount, double manualZoomPercent)
{
    const double minimum = ViewportDisplayLimits::minimumManualZoomPercent();
    const double maximum = ViewportDisplayLimits::maximumManualZoomPercent();
    const double base = std::clamp(manualZoomPercent, minimum, maximum);
    const double targetLog = std::log(base)
        + static_cast<double>(stepCount) * std::log(ViewportDisplayLimits::manualZoomStepFactor());
    if (!std::isfinite(targetLog) || targetLog >= std::log(maximum)) {
        return maximum;
    }
    if (targetLog <= std::log(minimum)) {
        return minimum;
    }
    return std::clamp(std::exp(targetLog), minimum, maximum);
}

bool commandHasOperation(const ViewportEnginePresentationCommand& command)
{
    return command.resetView() || command.hasFitMode() || command.hasManualZoomPercent()
        || command.hasZoomStepDelta() || command.hasContentPosition() || command.hasPanDelta()
        || command.hasContentAnchor() || command.hasRotationDegrees()
        || command.hasMirrorHorizontally() || command.hasMirrorVertically()
        || command.hasSpreadDirection() || command.hasPageGap() || command.hasBackgroundMode()
        || command.hasBackgroundColor() || command.hasCheckerboardLightColor()
        || command.hasCheckerboardDarkColor() || command.hasCheckerboardCellSize()
        || command.hasSmoothing() || command.hasMipmap() || command.hasLooping()
        || command.hasQualityPreference() || command.hasExactnessPreference();
}

bool commandValid(const ViewportEnginePresentationCommandInput& input)
{
    const ViewportEnginePresentationCommand& command = input.command;
    const bool resetConflicts = command.resetView()
        && (command.hasFitMode() || command.hasManualZoomPercent() || command.hasZoomStepDelta()
            || command.hasContentPosition() || command.hasPanDelta() || command.hasContentAnchor()
            || command.hasRotationDegrees() || command.hasMirrorHorizontally()
            || command.hasMirrorVertically() || command.hasSpreadDirection()
            || command.hasPageGap());
    const int geometryPositioningOperations = (command.hasManualZoomPercent() ? 1 : 0)
        + (command.hasZoomStepDelta() ? 1 : 0) + (command.hasContentPosition() ? 1 : 0)
        + (command.hasPanDelta() ? 1 : 0) + (command.hasContentAnchor() ? 1 : 0);
    const auto validRotation = [](int degrees) {
        return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
    };
    const double minimumZoom = ViewportDisplayLimits::minimumManualZoomPercent();
    const double maximumZoom = ViewportDisplayLimits::maximumManualZoomPercent();

    const bool relativeRotation = input.quarterTurnDelta != 0;
    return (commandHasOperation(command) || relativeRotation) && !resetConflicts
        && geometryPositioningOperations <= 1
        && (input.quarterTurnDelta == 0 || input.quarterTurnDelta == 1
            || input.quarterTurnDelta == -1)
        && !(relativeRotation && command.hasRotationDegrees())
        && ImageViewportInternal::isFinitePoint(input.anchor)
        && std::isfinite(input.geometry.devicePixelRatio) && input.geometry.devicePixelRatio > 0.0
        && (!command.hasFitMode() || ImageViewportInternal::isValidFitMode(command.fitMode()))
        && (!command.hasManualZoomPercent()
            || (ImageViewportInternal::isFinitePositive(command.manualZoomPercent())
                && command.manualZoomPercent() >= minimumZoom
                && command.manualZoomPercent() <= maximumZoom))
        && (!command.hasContentPosition()
            || ImageViewportInternal::isFinitePoint(command.contentPosition()))
        && (!command.hasPanDelta() || ImageViewportInternal::isFinitePoint(command.panDelta()))
        && (!command.hasContentAnchor()
            || ImageViewportInternal::isValidContentAnchor(command.contentAnchor()))
        && (!command.hasRotationDegrees() || validRotation(command.rotationDegrees()))
        && (!command.hasSpreadDirection()
            || ImageViewportInternal::isValidSpreadDirection(command.spreadDirection()))
        && (!command.hasPageGap()
            || (std::isfinite(command.pageGap()) && command.pageGap() >= 0.0
                && command.pageGap() <= ViewportDisplayLimits::maximumPageGap()))
        && (!command.hasBackgroundMode()
            || ImageViewportInternal::isValidBackgroundMode(command.backgroundMode()))
        && (!command.hasBackgroundColor() || command.backgroundColor().isValid())
        && (!command.hasCheckerboardLightColor() || command.checkerboardLightColor().isValid())
        && (!command.hasCheckerboardDarkColor() || command.checkerboardDarkColor().isValid())
        && (!command.hasCheckerboardCellSize()
            || (std::isfinite(command.checkerboardCellSize())
                && command.checkerboardCellSize()
                    >= ViewportDisplayLimits::minimumCheckerboardCellSize()
                && command.checkerboardCellSize()
                    <= ViewportDisplayLimits::maximumCheckerboardCellSize()))
        && (!command.hasQualityPreference()
            || ImageViewportInternal::isValidQualityPreference(command.qualityPreference()))
        && (!command.hasExactnessPreference()
            || ImageViewportInternal::isValidExactnessPreference(command.exactnessPreference()));
}

ViewportChangeSet presentationChanges(bool presentationChanged, bool displayChanged,
    bool affectsGeometry, bool readyDisplay, const QRectF& itemBounds)
{
    ViewportChangeSet changes;
    if (!presentationChanged) {
        return changes;
    }
    changes.displayRevision = displayChanged;
    changes.presentationRevision = presentationChanged;
    changes.geometryState = affectsGeometry && readyDisplay && !itemBounds.isEmpty();
    changes.scheduleUpdate = true;
    return changes;
}
}

bool validateViewportEnginePresentationCommand(const ViewportEnginePresentationCommandInput& input)
{
    return commandValid(input);
}

ViewportEnginePresentationCommandReduction reduceViewportEnginePresentationCommand(
    ViewportEnginePresentationCommandInput input, ViewportEnginePresentationCommandStateView state)
{
    ViewportEnginePresentationCommandReduction result;
    const PresentationState previousPresentation = state.presentation();
    const bool previousLooping = state.looping();
    PresentationState next = previousPresentation;
    bool nextLooping = previousLooping;
    bool affectsGeometry = false;
    const auto geometry = [&] { return projectViewportGeometryState(input.geometry, next); };
    const auto applyAnchored = [&](auto mutation) {
        const PresentationGeometry::State previousGeometry = geometry();
        mutation();
        preserveAnchoredContentPosition(next, input.geometry, previousGeometry, input.anchor);
        affectsGeometry = true;
    };
    const ViewportEnginePresentationCommand& command = input.command;

    if (command.resetView()) {
        next.fitMode = ImageViewportFitMode::Contain;
        next.manualZoom = 1.0;
        next.contentPosition = {};
        next.rotationDegrees = 0;
        next.mirrorHorizontally = false;
        next.mirrorVertically = false;
        affectsGeometry = true;
    }
    if (command.hasManualZoomPercent()) {
        const double manualZoom = command.manualZoomPercent() / 100.0;
        if (next.manualZoom != manualZoom) {
            if (next.fitMode == ImageViewportFitMode::Manual) {
                applyAnchored([&] { next.manualZoom = manualZoom; });
            } else {
                next.manualZoom = manualZoom;
            }
        }
    }
    if (command.hasZoomStepDelta()) {
        const double manualZoom
            = steppedZoomPercent(command.zoomStepDelta(), next.manualZoom * 100.0) / 100.0;
        if (next.manualZoom != manualZoom) {
            if (next.fitMode == ImageViewportFitMode::Manual) {
                applyAnchored([&] { next.manualZoom = manualZoom; });
            } else {
                next.manualZoom = manualZoom;
            }
        }
    }
    if (command.hasFitMode() && next.fitMode != command.fitMode()) {
        applyAnchored([&] { next.fitMode = command.fitMode(); });
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
    if (command.hasContentAnchor()) {
        QPointF requested;
        const PresentationGeometry::State currentGeometry = geometry();
        const QPointF maximum = PresentationGeometry::maximumContentPosition(currentGeometry);
        const bool rightToLeft = next.spreadDirection == ImageViewportSpreadDirection::RightToLeft;
        switch (command.contentAnchor()) {
        case ImageViewportContentAnchor::Start:
            requested = QPointF(rightToLeft ? maximum.x() : 0.0, 0.0);
            break;
        case ImageViewportContentAnchor::End:
            requested = QPointF(rightToLeft ? 0.0 : maximum.x(), maximum.y());
            break;
        }
        affectsGeometry = applyContentPosition(next, currentGeometry, requested) || affectsGeometry;
    }
    if (command.hasRotationDegrees() && next.rotationDegrees != command.rotationDegrees()) {
        applyAnchored([&] { next.rotationDegrees = command.rotationDegrees(); });
    }
    if (input.quarterTurnDelta != 0) {
        applyAnchored([&] {
            next.rotationDegrees = (next.rotationDegrees + input.quarterTurnDelta * 90 + 360) % 360;
        });
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
    if (command.hasCheckerboardLightColor()) {
        next.checkerboardLightColor = command.checkerboardLightColor();
    }
    if (command.hasCheckerboardDarkColor()) {
        next.checkerboardDarkColor = command.checkerboardDarkColor();
    }
    if (command.hasCheckerboardCellSize()) {
        next.checkerboardCellSize = command.checkerboardCellSize();
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
        return result;
    }
    if (presentationChanged) {
        result.presentation = next;
    }
    if (previousLooping != nextLooping) {
        result.looping = nextLooping;
    }
    result.targetPresentationChanged
        = !targetPresentationProjectionsEqual(previousPresentation, next);
    const bool displayChanged = !backingPresentationEqual(previousPresentation, next)
        || (state.readyDisplay() && result.targetPresentationChanged);
    result.changes = presentationChanges(
        changed, displayChanged, affectsGeometry, state.readyDisplay(), input.geometry.itemBounds);
    result.restageProviderDemands = affectsGeometry
        || previousPresentation.qualityPreference != next.qualityPreference
        || previousPresentation.exactnessPreference != next.exactnessPreference;
    return result;
}

ViewportEngineCommandTransition ViewportEngine::applyPresentationCommand(
    const ViewportEnginePresentationCommandRequest& input)
{
    ViewportEngineTransitionDraft transition;
    const ViewportEnginePresentationCommandInput operationInput { input.command, currentGeometry(),
        m_state->viewport.itemBounds.center(), 0 };
    if (!validateViewportEnginePresentationCommand(operationInput)) {
        return finalizeCommandTransition(rejectInvalidCommand(), std::move(transition));
    }

    const bool readyDisplay = m_state->displayState.display.hasReadyDisplay(
        m_state->requestState.request.roles[0].source.facts.present);
    const bool warningBefore = m_state->displayState.display.hasActiveRenderQualityFallback(
        m_state->requestState.request.sequenceGeneration, m_state->presentationState.presentation);
    ViewportEnginePresentationCommandStateView presentationState(
        m_state->presentationState.presentation, m_state->playbackState.playback.looping,
        readyDisplay);
    auto reduction
        = reduceViewportEnginePresentationCommand(operationInput, std::move(presentationState));
    if (!reduction.presentation && !reduction.looping) {
        return finalizeCommandTransition(
            acceptedPreservingCommandDiagnostics(), std::move(transition));
    }
    if (reduction.presentation) {
        m_state->presentationState.presentation = *reduction.presentation;
    }
    if (reduction.looping) {
        m_state->playbackState.playback.looping = *reduction.looping;
    }
    m_state->displayState.display.renderQualityFallback.reconcile(
        m_state->presentationState.presentation);
    const bool warningAfter = m_state->displayState.display.hasActiveRenderQualityFallback(
        m_state->requestState.request.sequenceGeneration, m_state->presentationState.presentation);
    if (warningBefore != warningAfter) {
        reduction.changes.diagnostics = true;
        reduction.changes.displayRevision = true;
    }
    if (reduction.targetPresentationChanged
        && m_state->requestState.presentationTarget.generation != 0) {
        advanceTargetPresentationRevision();
        reduction.changes.targetPresentationRevision = true;
        reduction.changes.adoptTargetPresentationRevision = readyDisplay;
    }
    transition.changes = reduction.changes;
    if (reduction.restageProviderDemands) {
        const auto providerEffects = restageProviderDemands(operationInput.geometry);
        const bool restaged = providerEffects[0].sendCommand || providerEffects[1].sendCommand;
        if (restaged) {
            transition.changes.requestState = true;
            transition.changes.requestRevision = true;
            transition.changes.displayState = true;
            transition.changes.displayRevision = true;
            transition.changes.scheduleUpdate = true;
        }
        appendProviderTransport(
            transition.providerTransport, providerEffects[0], ImageViewportPageRole::Primary);
        appendProviderTransport(
            transition.providerTransport, providerEffects[1], ImageViewportPageRole::Secondary);
    }
    return finalizeCommandTransition(accepted(), std::move(transition));
}

ViewportEnginePresentationTargetTransitionReduction
reduceViewportEnginePresentationTargetTransition(
    ViewportEnginePresentationTargetTransitionInput input,
    ViewportEnginePresentationTargetTransitionStateView state)
{
    ViewportEnginePresentationTargetTransitionReduction result;
    const PresentationState previousPresentation = state.presentation();
    PresentationState next = previousPresentation;
    if (input.zoomTransition
        == ViewportEnginePresentationTargetTransitionPolicy::ZoomTransition::ResetToContain) {
        next.fitMode = ImageViewportFitMode::Contain;
        next.manualZoom = 1.0;
    }
    if (input.explicitFitMode) {
        next.fitMode = *input.explicitFitMode;
    }
    if (input.rotationTransition
        == ViewportEnginePresentationTargetTransitionPolicy::RotationTransition::Reset) {
        next.rotationDegrees = 0;
    }
    if (input.mirrorTransition
        == ViewportEnginePresentationTargetTransitionPolicy::MirrorTransition::Reset) {
        next.mirrorHorizontally = false;
        next.mirrorVertically = false;
    }
    if (input.explicitSpreadDirection) {
        next.spreadDirection = *input.explicitSpreadDirection;
    }
    if (input.explicitPageGap) {
        next.pageGap = *input.explicitPageGap;
    }

    if (input.resolveContentPosition) {
        const PresentationGeometry::State acceptedGeometry
            = projectViewportGeometryState(input.acceptedGeometry, next);
        const QPointF maximum = PresentationGeometry::maximumContentPosition(acceptedGeometry);
        const bool rightToLeft = next.spreadDirection == ImageViewportSpreadDirection::RightToLeft;
        switch (input.contentPositionTransition) {
        case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::
            AnchorStart:
            applyContentPosition(
                next, acceptedGeometry, QPointF(rightToLeft ? maximum.x() : 0.0, 0.0));
            break;
        case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd:
            applyContentPosition(
                next, acceptedGeometry, QPointF(rightToLeft ? 0.0 : maximum.x(), maximum.y()));
            break;
        case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Clamp:
            applyContentPosition(next, acceptedGeometry, input.previousContentPosition);
            break;
        case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Invalid:
            break;
        }
    }

    const bool changed = !presentationStatesEqual(previousPresentation, next);
    if (!changed) {
        return result;
    }
    result.presentation = next;
    result.changes = presentationChanges(
        true, input.readyDisplay, true, input.readyDisplay, input.acceptedGeometry.itemBounds);
    return result;
}

ImageViewportInternal::ViewportChangeSet resolveViewportEnginePendingPresentationTargetTransition(
    const ViewportEngineGeometryInput& input, ViewportEnginePresentationTargetState& target,
    ImageViewportInternal::PresentationState& presentation, bool readyDisplay)
{
    const auto pending = target.pendingPresentationTransition;
    const bool completeRoleGeometry = input.primaryPresent && input.primarySize.isValid()
        && input.primarySize.width() > 0.0 && input.primarySize.height() > 0.0
        && (!target.acceptedRoleSet.secondary()
            || (input.secondarySize.isValid() && input.secondarySize.width() > 0.0
                && input.secondarySize.height() > 0.0));
    if (!pending.isValid() || pending.generation != target.generation || !completeRoleGeometry
        || input.itemBounds.isEmpty()) {
        return {};
    }

    const PresentationGeometry::State geometry = projectViewportGeometryState(input, presentation);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    const bool rightToLeft
        = presentation.spreadDirection == ImageViewportSpreadDirection::RightToLeft;
    bool changed = false;
    switch (pending.contentPositionTransition) {
    case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::AnchorStart:
        changed = applyContentPosition(
            presentation, geometry, QPointF(rightToLeft ? maximum.x() : 0.0, 0.0));
        break;
    case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd:
        changed = applyContentPosition(
            presentation, geometry, QPointF(rightToLeft ? 0.0 : maximum.x(), maximum.y()));
        break;
    case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Clamp:
        changed = applyContentPosition(presentation, geometry, pending.previousContentPosition);
        break;
    case ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Invalid:
        break;
    }
    target.pendingPresentationTransition = {};
    return presentationChanges(changed, readyDisplay, true, readyDisplay, input.itemBounds);
}
