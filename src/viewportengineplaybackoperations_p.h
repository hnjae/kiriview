#pragma once

#include "viewportenginestate_p.h"
#include "viewportplaybackcontract_p.h"

class ViewportEnginePlaybackScheduleAccess
{
    friend class ViewportEngine;
    ViewportEnginePlaybackScheduleAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        ViewportEngineProviderFactsView providerFacts)
        : m_request(request)
        , m_playback(playback)
        , m_providerFacts(providerFacts)
    {
    }

public:
    ViewportEnginePlaybackScheduleAccess(const ViewportEnginePlaybackScheduleAccess&) = delete;
    ViewportEnginePlaybackScheduleAccess(ViewportEnginePlaybackScheduleAccess&&) noexcept = default;
    ViewportEnginePlaybackScheduleAccess& operator=(const ViewportEnginePlaybackScheduleAccess&)
        = delete;

    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::PlaybackState& m_playback;
    ViewportEngineProviderFactsView m_providerFacts;
};

ViewportPlaybackScheduleEffect projectViewportPlaybackSchedule(
    ViewportEnginePlaybackScheduleAccess);
