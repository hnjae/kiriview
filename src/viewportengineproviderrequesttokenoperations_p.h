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

class ViewportProviderRequestTokenAllocationAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderSessionOpenedAccess;
    friend class ViewportEngineProviderQueueFlushAccess;
    friend class ViewportEngineProviderDemandRestageAccess;
    friend class ViewportEngineProviderFrameRequestAccess;

public:
    ViewportProviderRequestTokenAllocationAccess(std::array<ViewportEngineRoleState, 2>& roles,
        ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display)
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
    ImageViewportInternal::ProviderSessionState& session(ImageViewportPageRole role) const
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.session;
    }
    ImageViewportInternal::ProviderRequestLedger& requests(ImageViewportPageRole role) const
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.requests;
    }
    ViewportProviderFrameTransportEffect closeSession(ImageViewportPageRole role) const;
    ImageViewportInternal::RequestState& request() const { return m_request; }
    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    ImageViewportInternal::DisplayState& display() const { return m_display; }

private:
    std::array<ViewportEngineRoleState, 2>& m_roles;
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
};

ViewportProviderRequestTokenAllocationResult allocateViewportProviderRequestToken(
    ViewportProviderRequestTokenAllocationInput, ViewportProviderRequestTokenAllocationAccess);
