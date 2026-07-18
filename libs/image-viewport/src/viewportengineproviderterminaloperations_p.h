/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

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
struct ViewportEngineProviderTerminalProjectionMutation
{
    ImageViewportInternal::RequestState request;
};

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
    friend ImageViewportInternal::ViewportChangeSet
    reduceViewportEngineProviderDisplayRequestTerminalProjection(
        ViewportEngineProviderTerminalProjectionInput,
        ViewportEngineProviderTerminalProjectionAccess&);
    friend ImageViewportInternal::ViewportChangeSet
    reduceViewportEngineProviderGenerationTerminalProjection(
        ViewportEngineProviderTerminalProjectionInput,
        ViewportEngineProviderTerminalProjectionAccess&);

    explicit ViewportEngineProviderTerminalProjectionAccess(
        const ImageViewportInternal::RequestState& request)
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
    ViewportEngineProviderTerminalProjectionMutation takeMutation()
    {
        return { std::move(m_request) };
    }

private:
    ImageViewportInternal::RequestState m_request;
};

ImageViewportInternal::ViewportChangeSet
reduceViewportEngineProviderDisplayRequestTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput, ViewportEngineProviderTerminalProjectionAccess&);
ImageViewportInternal::ViewportChangeSet reduceViewportEngineProviderGenerationTerminalProjection(
    ViewportEngineProviderTerminalProjectionInput, ViewportEngineProviderTerminalProjectionAccess&);
