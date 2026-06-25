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
    return controller.clear();
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    return controller.play();
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    return controller.pause();
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    return controller.stop();
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    return controller.seek(frame);
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    return controller.seekToPosition(milliseconds);
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
