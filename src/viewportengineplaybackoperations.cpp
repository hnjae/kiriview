#include "viewportengineplaybackoperations_p.h"

#include <algorithm>

namespace {
const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}
}

ViewportPlaybackScheduleEffect projectViewportPlaybackSchedule(
    ViewportEnginePlaybackScheduleAccess access)
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    if (access.playback().phase != ImageViewport::PlaybackPhase::Playing
        || access.request().status != ImageViewport::RequestStatus::Ready) {
        return { Action::Stop, -1 };
    }

    const ImageViewport::PageRole role = access.playback().role;
    const auto& roleRequest = requestRole(access.request(), role);
    const auto& source = roleRequest.source;
    const auto& provider
        = access.providerFacts()[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.timingIntervals : source.facts.timingIntervals;
    const int frameCount = providerTiming ? intervals.frameCount() : source.facts.frameCount;
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
        = access.playback().position >= 0 ? access.playback().position : frameStart;
    return { Action::ArmAfter, std::max(1, frameStart + frameDuration - playbackPosition) };
}
