#pragma once

#include "imageviewport.h"

class ImageViewportPrivate;

class ViewportController
{
public:
    explicit ViewportController(ImageViewportPrivate &viewport);

    ImageViewport::CommandOutcome clear();
    ImageViewport::CommandOutcome play();
    ImageViewport::CommandOutcome pause();
    ImageViewport::CommandOutcome stop();
    ImageViewport::CommandOutcome seek(int frame);
    ImageViewport::CommandOutcome seekToPosition(int milliseconds);
    ImageViewport::CommandOutcome resetView();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
#endif

private:
    ImageViewportPrivate &viewport;
};
