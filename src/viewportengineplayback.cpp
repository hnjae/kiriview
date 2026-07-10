#include "viewportengine_p.h"

#include "viewportcontrollerplaybackcontract_p.h"

#include <algorithm>

namespace {

const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

}

ViewportPlaybackScheduleEffect ViewportEngine::playbackScheduleEffect() const
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Playing
        || m_requestState.status != ImageViewport::RequestStatus::Ready) {
        return { Action::Stop, -1 };
    }

    const ImageViewport::PageRole role = m_requestState.playbackRole;
    const auto& roleRequest = requestRole(m_requestState, role);
    const auto& source = roleRequest.source;
    const auto& provider = m_roles[roleIndex(role)].provider;
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.timingIntervals : source.facts.timingIntervals;
    const int frameCount
        = providerTiming ? intervals.frameCount() : source.facts.frameCount;
    const int totalDuration
        = providerTiming ? intervals.totalDuration() : source.facts.totalDuration;
    if (providerTiming && (!provider.metadataReady || !provider.timedMetadata)) {
        return { Action::Stop, -1 };
    }

    const int currentFrame = roleRequest.activeRequest.target.frame;
    if (currentFrame < 0 || currentFrame >= frameCount) {
        return { Action::Stop, -1 };
    }

    const int frameStart = intervals.frameStartPosition(currentFrame);
    const int nextFrameStart = currentFrame + 1 < frameCount
        ? intervals.frameStartPosition(currentFrame + 1)
        : totalDuration;
    const int frameDuration = nextFrameStart - frameStart;
    if (frameStart < 0 || frameDuration <= 0) {
        return { Action::Stop, -1 };
    }

    const int playbackPosition
        = m_requestState.playbackPosition >= 0 ? m_requestState.playbackPosition : frameStart;
    return { Action::ArmAfter,
        std::max(1, frameStart + frameDuration - playbackPosition) };
}
