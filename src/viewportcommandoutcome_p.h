#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportengine_p.h"

namespace ImageViewportInternal::CommandOutcome {

ViewportCommandResult fromEngineCommand(const ViewportEngine::CommandResult& command);

}
