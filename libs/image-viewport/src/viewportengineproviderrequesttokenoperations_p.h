/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginestate_p.h"
#include "viewportprovidercontract_p.h"

struct ViewportProviderRequestTokenAllocationInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
};

struct ViewportProviderRequestTokenAllocationResult
{
    ImageSequenceProviderRequestToken token;
    bool exhausted = false;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportProviderRequestTokenAllocationMutation
{
    std::array<ViewportEngineRoleState, 2> roles;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
};

class ViewportProviderRequestTokenAllocationAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderSessionOpenedAccess;
    friend class ViewportEngineProviderQueueFlushAccess;
    friend class ViewportEngineProviderDemandRestageAccess;
    friend class ViewportEngineProviderFrameRequestAccess;

public:
    ViewportProviderRequestTokenAllocationAccess(
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display)
        : m_roles(roles)
        , m_request(request)
        , m_playback(playback)
        , m_display(display)
    {
    }

    ViewportProviderRequestTokenAllocationAccess(
        const ViewportProviderRequestTokenAllocationAccess&)
        = delete;
    ViewportProviderRequestTokenAllocationAccess(
        ViewportProviderRequestTokenAllocationAccess&&) noexcept
        = default;
    ViewportProviderRequestTokenAllocationAccess& operator=(
        const ViewportProviderRequestTokenAllocationAccess&)
        = delete;
    ImageViewportInternal::ProviderSessionState& session(ImageViewportPageRole role)
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.session;
    }
    ImageViewportInternal::ProviderRequestLedger& requests(ImageViewportPageRole role)
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.requests;
    }
    ViewportProviderFrameTransportEffect closeSession(ImageViewportPageRole role);
    ViewportProviderRequestTokenAllocationMutation takeMutation()
    {
        return { std::move(m_roles), std::move(m_request), std::move(m_playback),
            std::move(m_display) };
    }
    ImageViewportInternal::RequestState& request() { return m_request; }
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    ImageViewportInternal::DisplayState& display() { return m_display; }

private:
    std::array<ViewportEngineRoleState, 2> m_roles;
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
};

ViewportProviderRequestTokenAllocationResult allocateViewportProviderRequestToken(
    ViewportProviderRequestTokenAllocationInput, ViewportProviderRequestTokenAllocationAccess&);
