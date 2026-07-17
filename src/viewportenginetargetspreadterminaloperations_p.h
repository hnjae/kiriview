#pragma once

#include "viewportenginestate_p.h"

struct ViewportEngineTargetSpreadTerminalInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::Error;
    ImageViewportRequestReason reason = ImageViewportRequestReason::ProviderFailure;
    ImageViewportInternal::FailureScope scope = ImageViewportInternal::FailureScope::None;
    QString diagnostic;
    ImageViewportInternal::ViewportChangeSet changes;
};

ImageViewportInternal::ViewportChangeSet recordViewportEngineTargetSpreadTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);
