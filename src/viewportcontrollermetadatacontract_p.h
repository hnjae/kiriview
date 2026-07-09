#pragma once

#include "imageviewport.h"

struct ViewportMetadataProjection
{
    int frameCount = -1;
    int totalDuration = -1;
    ImageViewportRange frameSeekBounds;
    ImageViewportRange positionSeekBounds;
    ImageViewport::CapabilitySupport timedPlaybackSupport
        = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport frameSeekSupport
        = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport positionSeekSupport
        = ImageViewport::CapabilitySupport::Unavailable;
};
