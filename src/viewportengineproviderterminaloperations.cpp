#include "viewportengineproviderterminaloperations_p.h"

#include <utility>

ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput input,
    ViewportEngineProviderTerminalProjectionAccess access)
{
    return recordViewportEngineTargetSpreadTerminal(std::move(input), access.m_request);
}
