#pragma once

#include "imageviewport.h"

struct ViewportMetadataProjection
{
    int frameCount = -1;
    int totalDuration = -1;
    ImageViewportRange frameSeekBounds;
    ImageViewportRange positionSeekBounds;
    ImageViewport::TriState timedPlaybackSupport = ImageViewport::TriState::Unavailable;
    ImageViewport::TriState frameSeekSupport = ImageViewport::TriState::Unavailable;
    ImageViewport::TriState positionSeekSupport = ImageViewport::TriState::Unavailable;
};
