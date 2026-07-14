#pragma once

#include "imageviewportstate_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportengine_p.h"
#include <ImageViewport/ImageViewport>

namespace ImageViewportInternal::CommandOutcome {

ViewportCommandResult fromEngineCommand(const ViewportEngineCommandResult& command);

}
