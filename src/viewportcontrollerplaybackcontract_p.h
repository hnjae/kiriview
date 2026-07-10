#pragma once

#include "imageviewportstate_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportPlaybackAdvanceResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
    ViewportPlaybackScheduleEffect schedule;
};
