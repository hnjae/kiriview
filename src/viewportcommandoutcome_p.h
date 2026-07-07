#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

class ViewportControllerPort;
struct ViewportCommandResult;

namespace ImageViewportInternal::CommandOutcome {

void markAccepted(ViewportControllerPort& viewport, ViewportCommandResult& result);
void markRejected(ViewportControllerPort& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason);

ViewportCommandResult accepted(ViewportControllerPort& viewport);
ViewportCommandResult accepted(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet changes);
ViewportCommandResult rejected(ViewportControllerPort& viewport,
    ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
ViewportCommandResult invalid(ViewportControllerPort& viewport);
ViewportCommandResult unsupported(ViewportControllerPort& viewport);
ViewportCommandResult ignoredNoRequest(ViewportControllerPort& viewport);

}
