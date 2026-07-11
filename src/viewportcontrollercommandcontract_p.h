#pragma once

#include "imageviewport.h"
#include "viewportcontrollertransition_p.h"

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ViewportControllerTransition transition;
};

struct ViewportPresentationCommandInput
{
    ImageViewportPresentationCommand command;
    QPointF anchor;
    double devicePixelRatio = 1.0;
};
