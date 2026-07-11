#include "viewportcommandoutcome_p.h"
#include "viewportcontroller_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerhelpers_p.h"
#include "viewportprovidertransporteffects_p.h"

ViewportCommandResult ViewportController::setPresentation(
    const ViewportPresentationCommandInput& input)
{
    const ViewportEngine::PresentationCommandResult engineResult
        = engine.applyPresentationCommand({ input.command,
            engine.projectedGeometryInput(itemBounds(), input.devicePixelRatio), input.anchor });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    mergeChanges(result.transition.changes, engineResult.changes);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[1], ImageViewport::PageRole::Secondary);
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
    ViewportPresentationCommandInput input { command, anchor, 1.0 };
    const auto engineResult = engine.applyPresentationCommand(
        { input.command, engine.projectedGeometryInput(itemBounds()), input.anchor, 1 });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    mergeChanges(result.transition.changes, engineResult.changes);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[1], ImageViewport::PageRole::Secondary);
    return result;
}

ViewportCommandResult ViewportController::rotateCounterClockwise(QPointF anchor)
{
    ImageViewportPresentationCommand command;
    const auto engineResult = engine.applyPresentationCommand(
        { command, engine.projectedGeometryInput(itemBounds()), anchor, -1 });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    mergeChanges(result.transition.changes, engineResult.changes);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[1], ImageViewport::PageRole::Secondary);
    return result;
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
