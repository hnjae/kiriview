#include "viewportcontroller_p.h"

#include "viewportcontrollercommandcontract_p.h"
#include "viewportprovidertransporteffects_p.h"

ViewportCommandResult ViewportController::applyPlaybackCommand(ViewportPlaybackCommand command)
{
    const ViewportEnginePlaybackCommandResult engineResult
        = engine.applyPlaybackCommand({ command, { itemBounds(), 1.0 } });
    ViewportCommandResult result;
    result.outcome = engineResult.command.outcome;
    result.transition.changes = engineResult.changes;
    appendProviderTransport(result.transition.providerBeforePublication,
        engineResult.effects.providerFrameTransport[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.transition.providerBeforePublication,
        engineResult.effects.providerFrameTransport[1], ImageViewport::PageRole::Secondary);
    result.transition.playbackSchedule = engineResult.schedule;
    return result;
}

ViewportCommandResult ViewportController::play(ImageViewport::PageRole role)
{
    return applyPlaybackCommand({ ViewportPlaybackCommand::Kind::Play, role });
}

ViewportCommandResult ViewportController::pause(ImageViewport::PageRole role)
{
    return applyPlaybackCommand({ ViewportPlaybackCommand::Kind::Pause, role });
}

ViewportCommandResult ViewportController::stop(ImageViewport::PageRole role)
{
    return applyPlaybackCommand({ ViewportPlaybackCommand::Kind::Stop, role });
}

ViewportCommandResult ViewportController::seek(ImageViewport::PageRole role, int frame)
{
    return applyPlaybackCommand({ ViewportPlaybackCommand::Kind::SeekFrame, role, frame });
}

ViewportCommandResult ViewportController::seekToPosition(
    ImageViewport::PageRole role, int milliseconds)
{
    return applyPlaybackCommand(
        { ViewportPlaybackCommand::Kind::SeekPosition, role, milliseconds });
}

ViewportControllerTransition ViewportController::advancePlayback(int elapsedMilliseconds)
{
    const ViewportEnginePlaybackTickResult engineResult
        = engine.advancePlayback({ elapsedMilliseconds, { itemBounds(), 1.0 } });
    ViewportControllerTransition result;
    result.changes = engineResult.changes;
    appendProviderTransport(result.providerBeforePublication,
        engineResult.effects.providerFrameTransport[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.providerBeforePublication,
        engineResult.effects.providerFrameTransport[1], ImageViewport::PageRole::Secondary);
    result.playbackSchedule = engineResult.schedule;
    return result;
}
