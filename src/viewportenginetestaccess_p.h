#pragma once

#include "viewportengine_p.h"

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class ViewportEngineTestAccess
{
public:
    static ImageViewportInternal::DisplayState& display(ViewportEngine& engine)
    {
        return engine.displayState();
    }
    static const ImageViewportInternal::DisplayState& display(const ViewportEngine& engine)
    {
        return engine.displayState();
    }
    static ImageViewportInternal::RequestState& request(ViewportEngine& engine)
    {
        return engine.requestState();
    }
    static const ImageViewportInternal::RequestState& request(const ViewportEngine& engine)
    {
        return engine.requestState();
    }
    static ImageViewportInternal::ProviderGenerationState& provider(
        ViewportEngine& engine, ImageViewport::PageRole role)
    {
        return engine.providerState(role);
    }
    static const ImageViewportInternal::ProviderGenerationState& provider(
        const ViewportEngine& engine, ImageViewport::PageRole role)
    {
        return engine.providerState(role);
    }
};
#endif
