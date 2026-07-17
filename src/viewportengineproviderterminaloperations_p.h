#pragma once

#include "viewportenginetargetspreadterminaloperations_p.h"

using ViewportEngineProviderTerminalProjectionInput = ViewportEngineTargetSpreadTerminalInput;

class ViewportEngineProviderMetadataReadyAccess;
class ViewportEngineProviderFrameReadyAccess;
class ViewportEngineProviderTerminalEventAccess;
class ViewportEngineProviderProtocolViolationAccess;
class ViewportEngineProviderDispatchFailureAccess;
class ViewportEngineProviderSessionOpenFailureAccess;
class ViewportEngineProviderQueueFailureAccess;

class ViewportEngineProviderTerminalProjectionAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderMetadataReadyAccess;
    friend class ViewportEngineProviderFrameReadyAccess;
    friend class ViewportEngineProviderTerminalEventAccess;
    friend class ViewportEngineProviderProtocolViolationAccess;
    friend class ViewportEngineProviderDispatchFailureAccess;
    friend class ViewportEngineProviderSessionOpenFailureAccess;
    friend class ViewportEngineProviderQueueFailureAccess;
    friend ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderTerminalProjection(
        ViewportEngineProviderTerminalProjectionInput,
        ViewportEngineProviderTerminalProjectionAccess);

    explicit ViewportEngineProviderTerminalProjectionAccess(
        ImageViewportInternal::RequestState& request)
        : m_request(request)
    {
    }

public:
    ViewportEngineProviderTerminalProjectionAccess(
        const ViewportEngineProviderTerminalProjectionAccess&)
        = delete;
    ViewportEngineProviderTerminalProjectionAccess(
        ViewportEngineProviderTerminalProjectionAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::RequestState& m_request;
};

ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput, ViewportEngineProviderTerminalProjectionAccess);
