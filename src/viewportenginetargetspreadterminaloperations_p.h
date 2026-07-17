#pragma once

#include "viewportenginestate_p.h"

struct ViewportEngineTargetSpreadTerminalInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::Error;
    ImageViewportRequestReason reason = ImageViewportRequestReason::ProviderFailure;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
    ImageViewportInternal::ViewportChangeSet changes;
};

ImageViewportInternal::ViewportChangeSet recordViewportEngineDisplayRequestTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);
ImageViewportInternal::ViewportChangeSet recordViewportEngineGenerationTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);

bool viewportEngineHasCurrentDisplayRequestTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineHasCurrentGenerationTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineHasCurrentTerminal(const ImageViewportInternal::RequestState&);
