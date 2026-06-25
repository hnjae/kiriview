#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

ViewportController::ViewportController(ImageViewportPrivate &viewport)
    : viewport(viewport)
{
}

ImageViewport::CommandOutcome ViewportController::clear()
{
    return viewport.clearCommandImpl();
}

ImageViewport::CommandOutcome ViewportController::play()
{
    return viewport.playCommandImpl();
}

ImageViewport::CommandOutcome ViewportController::pause()
{
    return viewport.pauseCommandImpl();
}

ImageViewport::CommandOutcome ViewportController::stop()
{
    return viewport.stopCommandImpl();
}

ImageViewport::CommandOutcome ViewportController::seek(int frame)
{
    return viewport.seekCommandImpl(frame);
}

ImageViewport::CommandOutcome ViewportController::seekToPosition(int milliseconds)
{
    return viewport.seekToPositionCommandImpl(milliseconds);
}

ImageViewport::CommandOutcome ViewportController::resetView()
{
    return viewport.resetViewCommandImpl();
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::advancePlaybackForTest(int elapsedMilliseconds)
{
    viewport.advancePlaybackForTestImpl(elapsedMilliseconds);
}
#endif

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.clear();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.play();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.pause();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.stop();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.seek(frame);
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.seekToPosition(milliseconds);
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    return controller.resetView();
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    controller.advancePlaybackForTest(elapsedMilliseconds);
}
#endif
