#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollergeometryhelpers_p.h"

ImageViewportInternal::ViewportChangeSet ViewportController::applyPresentationTransition(
    const ControllerTransitionPolicy& policy, QPointF previousContentPosition,
    double previousZoomPercent)
{
    return state.engine.applyPresentationTargetTransition(
        { policy.magnificationPolicy, policy.contentPositionTransition, policy.rotationTransition,
            policy.mirrorTransition, policy.explicitFitMode, policy.explicitSpreadDirection,
            policy.explicitPageGap, acceptedGeometryInput(viewport), previousContentPosition,
            previousZoomPercent, viewport.hasReadyDisplay() });
}

ViewportCommandResult ViewportController::setPresentation(
    const ViewportPresentationCommandInput& input)
{
    const ViewportEngine::PresentationCommandResult engineResult
        = state.engine.applyPresentationCommand(
            { input.command, controllerGeometryInput(viewport, input.devicePixelRatio),
                input.anchor, viewport.hasReadyDisplay() });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(viewport, engineResult.command);
    mergeChanges(result.changes, engineResult.changes);
    return result;
}

ViewportCommandResult ViewportController::setSpreadDirection(
    ImageViewport::SpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::setPageGap(double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::setFitMode(ImageViewport::FitMode mode, QPointF anchor)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
    return setPresentation({ command, anchor, 1.0 });
}

ViewportCommandResult ViewportController::setZoomPercent(
    double percent, QPointF anchor, double devicePixelRatio)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return setPresentation({ command, anchor, devicePixelRatio });
}

ViewportCommandResult ViewportController::zoomByStep(
    int stepCount, QPointF anchor, double devicePixelRatio)
{
    ImageViewportPresentationCommand command;
    command.setZoomStepDelta(stepCount);
    return setPresentation({ command, anchor, devicePixelRatio });
}

ViewportCommandResult ViewportController::panBy(QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::panToStart()
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(ImageViewport::ScanDirection::Start);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::panToEnd()
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(ImageViewport::ScanDirection::End);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::scanNext()
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(ImageViewport::ScanDirection::Next);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::scanPrevious()
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(ImageViewport::ScanDirection::Previous);
    return setPresentation({ command, {}, 1.0 });
}

ViewportCommandResult ViewportController::rotateClockwise(QPointF anchor)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees((state.engine.presentationState().rotationDegrees + 90) % 360);
    return setPresentation({ command, anchor, 1.0 });
}

ViewportCommandResult ViewportController::rotateCounterClockwise(QPointF anchor)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees((state.engine.presentationState().rotationDegrees + 270) % 360);
    return setPresentation({ command, anchor, 1.0 });
}

ViewportCommandResult ViewportController::setMirrorHorizontally(bool enabled, QPointF anchor)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(enabled);
    return setPresentation({ command, anchor, 1.0 });
}

ViewportCommandResult ViewportController::setMirrorVertically(bool enabled, QPointF anchor)
{
    ImageViewportPresentationCommand command;
    command.setMirrorVertically(enabled);
    return setPresentation({ command, anchor, 1.0 });
}

ViewportCommandResult ViewportController::resetView()
{
    return setPresentation({ ImageViewportPresentationCommand::resetViewCommand(), {}, 1.0 });
}
