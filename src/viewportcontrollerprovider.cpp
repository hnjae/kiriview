#include "viewportcontroller_p.h"

ViewportControllerTransition ViewportController::handleProviderHostEvent(
    const ViewportProviderHostEvent& event)
{
    return engine.handleProviderHostEvent({ event, { itemBounds(), 1.0 } });
}

ViewportControllerTransition ViewportController::handleDevicePixelRatioChanged(
    double devicePixelRatio)
{
    return engine.handleDevicePixelRatioChanged({ itemBounds(), devicePixelRatio });
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return engine.closeProviderSession(role);
}
