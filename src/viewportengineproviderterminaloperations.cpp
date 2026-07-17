#include "viewportengineproviderterminaloperations_p.h"

#include <utility>

ImageViewportInternal::ViewportChangeSet
reduceViewportEngineProviderDisplayRequestTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput input,
    ViewportEngineProviderTerminalProjectionAccess access)
{
    return recordViewportEngineDisplayRequestTerminal(std::move(input), access.m_request);
}

ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderGenerationTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput input,
    ViewportEngineProviderTerminalProjectionAccess access)
{
    return recordViewportEngineGenerationTerminal(std::move(input), access.m_request);
}
