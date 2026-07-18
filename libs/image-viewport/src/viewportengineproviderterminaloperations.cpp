// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineproviderterminaloperations_p.h"

#include <utility>

ImageViewportInternal::ViewportChangeSet
reduceViewportEngineProviderDisplayRequestTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput input,
    ViewportEngineProviderTerminalProjectionAccess& access)
{
    return recordViewportEngineDisplayRequestTerminal(std::move(input), access.m_request);
}

ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderGenerationTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput input,
    ViewportEngineProviderTerminalProjectionAccess& access)
{
    return recordViewportEngineGenerationTerminal(std::move(input), access.m_request);
}
