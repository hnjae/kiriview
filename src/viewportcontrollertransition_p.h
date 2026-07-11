#pragma once

#include "imageviewportstate_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportControllerTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderTransportBatch providerBeforePublication;
    ViewportProviderTransportBatch providerAfterPublication;
    ViewportPlaybackScheduleEffect playbackSchedule;
    ImageViewportInternal::ProviderSchedulerDiagnostic providerSchedulerDiagnostic;
};
