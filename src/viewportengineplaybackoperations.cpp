#include "viewportengineplaybackoperations_p.h"

#include "imageviewportproviderfacts_p.h"

#include <algorithm>

namespace {
const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}
}

bool validateViewportPlaybackCommand(ViewportPlaybackCommand command)
{
    switch (command.kind) {
    case ViewportPlaybackCommand::Kind::Play:
    case ViewportPlaybackCommand::Kind::Pause:
    case ViewportPlaybackCommand::Kind::Stop:
    case ViewportPlaybackCommand::Kind::SeekFrame:
    case ViewportPlaybackCommand::Kind::SeekPosition:
        break;
    default:
        return false;
    }
    switch (command.role) {
    case ImageViewport::PageRole::Primary:
    case ImageViewport::PageRole::Secondary:
        return true;
    default:
        return false;
    }
}

ViewportEnginePlaybackPauseReduction reduceViewportEnginePlaybackPause(
    ViewportEnginePlaybackPauseInput input, ViewportEnginePlaybackPauseAccess access)
{
    ViewportEnginePlaybackPauseReduction result;
    if (access.playback().role != input.role
        || (access.playback().phase != ImageViewport::PlaybackPhase::Playing
            && access.playback().phase != ImageViewport::PlaybackPhase::Waiting)) {
        return result;
    }
    access.playback().phase = ImageViewport::PlaybackPhase::Paused;
    result.playbackPhaseChanged = true;
    return result;
}

ViewportEngineAuthoredAutoplayReduction reduceViewportEngineAuthoredAutoplay(
    ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess access)
{
    ViewportEngineAuthoredAutoplayReduction result;
    const auto& source = access.source();
    const auto facts = source.facts.provider ? access.providerFacts().authoredAnimationFacts
                                             : source.facts.authoredAnimationFacts;
    if (!facts.autoplay()) {
        return result;
    }
    if (source.facts.provider
        && ImageViewportInternal::providerCapabilityKnownFalse(
            source.facts.providerTimedPlaybackCapability)) {
        return result;
    }
    if (!source.facts.provider
        && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
        return result;
    }

    auto& playback = access.playback();
    const auto previousPhase = playback.phase;
    playback.role = ImageViewport::PageRole::Primary;
    playback.stopWhenRequestReady = false;
    playback.loopIterationsCompleted = 0;
    result.armed = true;
    result.playbackChanged = true;

    if (source.facts.provider) {
        if (!access.providerFacts().metadataReady) {
            playback.providerStartPending = true;
            access.activeRequest().target
                = { -1, -1, ImageViewportInternal::ProviderRequestTargetKind::Playback };
            access.activeRequest().resolvedFrame = { -1, -1 };
            playback.position = -1;
            playback.phase = ImageViewport::PlaybackPhase::Waiting;
            result.activeRequestChanged = true;
        } else if (access.providerFacts().timedMetadata
            && access.providerFacts().timedPlaybackSupport) {
            const int frame = access.activeRequest().target.frame;
            playback.position = access.providerFacts().timingIntervals.frameStartPosition(frame);
            playback.phase = access.requestStatus() == ImageViewport::RequestStatus::Loading
                ? ImageViewport::PlaybackPhase::Waiting
                : ImageViewport::PlaybackPhase::Playing;
        }
        result.playbackPhaseChanged = previousPhase != playback.phase;
        return result;
    }

    playback.position
        = source.facts.timingIntervals.frameStartPosition(access.activeRequest().target.frame);
    playback.phase = access.requestStatus() == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
    result.playbackPhaseChanged = previousPhase != playback.phase;
    return result;
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
