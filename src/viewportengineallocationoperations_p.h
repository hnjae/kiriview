#pragma once

#include "imageviewportstate_p.h"

struct ViewportEnginePayloadAllocationRebuildResult
{
    bool retainedDisplayDiscarded = false;
    bool roleBudgetsIncreased = false;
};

qint64 viewportEngineDisplayPayloadByteBudget();
ViewportEnginePayloadAllocationRebuildResult rebuildViewportEnginePayloadAllocation(
    const ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display);
