#include "viewportcontroller_p.h"

#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerplaybackcontract_p.h"

ViewportCommandResult ViewportController::applyPlaybackCommand(ViewportPlaybackCommand command)
{
    const ViewportEngine::PlaybackCommandResult engineResult
        = engine.applyPlaybackCommand({ command, engine.acceptedGeometryInput(itemBounds()) });
    ViewportCommandResult result;
    result.outcome = engineResult.command.outcome;
    result.changes = engineResult.changes;
    result.providerFrameTransport = engineResult.effects.providerFrameTransport[0];
    result.secondaryProviderFrameTransport = engineResult.effects.providerFrameTransport[1];
    result.playbackSchedule = engineResult.schedule;
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

ViewportPlaybackScheduleEffect ViewportController::playbackScheduleEffect() const
{
    return engine.playbackScheduleEffect();
}

ViewportPlaybackAdvanceResult ViewportController::advancePlayback(int elapsedMilliseconds)
{
    const ViewportEngine::PlaybackTickResult engineResult
        = engine.advancePlayback({ elapsedMilliseconds, engine.acceptedGeometryInput(itemBounds()) });
    return { engineResult.changes, engineResult.effects.providerFrameTransport[0],
        engineResult.effects.providerFrameTransport[1], engineResult.schedule };
}
