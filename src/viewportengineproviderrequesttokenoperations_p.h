#pragma once

#include "viewportprovidercontract_p.h"
#include "viewportenginestate_p.h"

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
    friend class ViewportEnginePlaybackStopAccess;
    friend class ViewportEnginePlaybackSeekAccess;
    friend class ViewportEnginePlaybackPlayAccess;
    friend class ViewportEnginePlaybackTickAccess;
    friend class ViewportEngineProviderSessionOpenedAccess;
    friend class ViewportEngineProviderQueueFlushAccess;
    friend class ViewportEngineProviderDemandRestageAccess;
    friend class ViewportEngineProviderFrameRequestAccess;
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

public:
    ViewportProviderRequestTokenAllocationAccess(
        const ViewportProviderRequestTokenAllocationAccess&)
        = delete;
    ViewportProviderRequestTokenAllocationAccess& operator=(
        const ViewportProviderRequestTokenAllocationAccess&)
        = delete;
    ImageViewportInternal::ProviderSessionState& session(ImageViewportPageRole role) const
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.session;
    }
    ImageViewportInternal::ProviderRequestState& requests(ImageViewportPageRole role) const
    {
        return m_roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider.requests;
    }
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
