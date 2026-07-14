#pragma once

#include "viewportcontrollertransition_p.h"
#include <ImageViewport/ImageViewport>

struct ViewportCommandResult
{
    ImageViewportCommandOutcome outcome = ImageViewportCommandOutcome::Accepted;
    ViewportControllerTransition transition;
};

struct ViewportPresentationCommandInput
{
    ImageViewportPresentationCommand command;
    QPointF anchor;
    double devicePixelRatio = 1.0;
};
