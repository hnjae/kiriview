#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportcontrollerplaybackcontract_p.h"

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderTransportBatch beforeChanges;
    ViewportProviderTransportBatch afterChanges;
    ViewportPlaybackScheduleEffect playbackSchedule;
};

struct ViewportPresentationCommandInput
{
    ImageViewportPresentationCommand command;
    QPointF anchor;
    double devicePixelRatio = 1.0;
};
