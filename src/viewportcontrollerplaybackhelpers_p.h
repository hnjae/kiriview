#pragma once

#include "viewportcontrollercorehelpers_p.h"

#include "playbacktimeline_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerplaybackcontract_p.h"

namespace {
using ImageViewportInternal::PlaybackAdvanceTarget;
using ImageViewportInternal::playbackAdvanceTarget;

struct ViewportPlaybackRoleTiming
{
    bool valid = false;
    bool provider = false;
    int frameCount = -1;
    int totalDuration = -1;
    TimingIntervals timingIntervals;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;

    int frameStartPosition(int frame) const
    {
        return valid ? timingIntervals.frameStartPosition(frame) : -1;
    }

    int frameIndexForPosition(int position) const
    {
        return valid ? timingIntervals.frameIndexForPosition(position) : -1;
    }
};

ViewportCommandResult commandResultWithSecondaryTransport(ViewportCommandResult result)
{
    result.secondaryProviderFrameTransport = result.providerFrameTransport;
    result.providerFrameTransport = {};
    return result;
}

ViewportCommandResult commandResultWithRoleTransport(
    ImageViewport::PageRole role, ViewportCommandResult result)
{
    return role == ImageViewport::PageRole::Secondary ? commandResultWithSecondaryTransport(result)
                                                      : result;
}

ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapabilityForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary
        ? viewport.secondaryProviderTimedPlaybackCapability()
        : viewport.providerTimedPlaybackCapability();
}

ImageSequenceProviderCapabilitySupport providerFrameSeekCapabilityForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary
        ? viewport.secondaryProviderFrameSeekCapability()
        : viewport.providerFrameSeekCapability();
}

ImageSequenceProviderCapabilitySupport providerPositionSeekCapabilityForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary
        ? viewport.secondaryProviderPositionSeekCapability()
        : viewport.providerPositionSeekCapability();
}

ImageSequenceProviderKnownFacts providerKnownFactsForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? viewport.secondaryProviderKnownFacts()
                                                      : viewport.providerKnownFacts();
}

bool shouldPreservePlaybackPositionOnPlay(
    ImageViewport::PlaybackPhase phase, bool stopWhenRequestReady)
{
    return !stopWhenRequestReady
        && (phase == ImageViewport::PlaybackPhase::Playing
            || phase == ImageViewport::PlaybackPhase::Paused
            || phase == ImageViewport::PlaybackPhase::Waiting);
}

bool hasTimedBuiltInSequenceForRole(ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    if (hasProviderSequenceForRole(viewport, role)) {
        return false;
    }
    const ImageViewportInternal::ImageSequenceSource& source
        = sequenceSourceForRole(viewportRequestState(viewport), role);
    return source.sequence && source.facts.timed && source.facts.timingIntervals.isValid();
}

ViewportPlaybackRoleTiming playbackTimingForRole(
    ViewportControllerPort viewport, const ViewportControllerState& state,
    ImageViewport::PageRole role)
{
    if (hasProviderSequenceForRole(viewport, role)) {
        const ImageViewportInternal::ProviderGenerationState& provider
            = providerRoleStateFor(state, role).provider;
        if (!provider.metadataReady || !provider.timedMetadata) {
            return {};
        }
        return { true, true, provider.timingIntervals.frameCount(),
            provider.timingIntervals.totalDuration(), provider.timingIntervals,
            provider.authoredAnimationFacts };
    }

    if (!hasTimedBuiltInSequenceForRole(viewport, role)) {
        return {};
    }
    const ImageViewportInternal::ImageSequenceSource& source
        = sequenceSourceForRole(viewportRequestState(viewport), role);
    return { true, false, source.facts.timingIntervals.frameCount(),
        source.facts.timingIntervals.totalDuration(), source.facts.timingIntervals,
        source.facts.authoredAnimationFacts };
}

int providerFrameStartPositionForRole(
    const ViewportControllerState& state, ImageViewport::PageRole role, int frame)
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    return provider.timingIntervals.frameStartPosition(frame);
}

int providerFrameIndexForPositionForRole(
    const ViewportControllerState& state, ImageViewport::PageRole role, int position)
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    return provider.timingIntervals.frameIndexForPosition(position);
}

const ViewportSequenceRoleSource& secondaryRoleSource(const ViewportControllerState& state)
{
    return state.secondarySource;
}

DisplayRequestTarget providerPlaybackStartTarget(
    ViewportControllerPort& viewport, const ViewportControllerState& state,
    ImageViewport::PageRole role)
{
    int selectedFrame = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (selectedFrame < 0
        || selectedFrame >= provider.timingIntervals.frameCount()) {
        selectedFrame = 0;
    }
    return DisplayRequestTarget { selectedFrame,
        providerFrameStartPositionForRole(state, role, selectedFrame),
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

DisplayRequestTarget pendingProviderPlaybackTarget()
{
    return DisplayRequestTarget { -1, -1,
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

void applyPendingProviderPlaybackTargetForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role, DisplayRequestTarget target)
{
    ImageViewportInternal::RequestState& request = viewportRequestState(viewport);
    ImageViewportInternal::DisplayRequest& activeRequest = activeRequestForRole(request, role);
    if (role == ImageViewport::PageRole::Primary) {
        request.providerPlaybackStartPending = true;
        activeRequest.target.frame = target.frame;
        activeRequest.target.position = target.position;
        activeRequest.resolvedFrame = { -1, -1 };
        request.playbackPosition = target.position;
        activeRequest.target.providerTargetKind = target.providerTargetKind;
        return;
    }

    activeRequest.target = target;
    activeRequest.resolvedFrame = { -1, -1 };
    activeRequest.providerFrameToken = {};
    request.playbackPosition = target.position;
}

void setPlaybackProviderFrameTransportForRole(ViewportPlaybackAdvanceResult& result,
    ImageViewport::PageRole role, ViewportProviderFrameTransportEffect transport)
{
    if (role == ImageViewport::PageRole::Secondary) {
        result.secondaryProviderFrameTransport = transport;
        return;
    }
    result.providerFrameTransport = transport;
}

ImageViewport::PlaybackPhase playbackPhaseForCurrentRequest(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

bool effectiveLoopingForPlayback(
    ViewportControllerPort& viewport, ImageSequenceAuthoredAnimationFacts facts)
{
    if (viewportRequestState(viewport).looping) {
        return true;
    }

    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return viewportRequestState(viewport).playbackLoopIterationsCompleted + 1
            < facts.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

void updateLoopProgressForAcceptedPlaybackTarget(
    ViewportControllerPort& viewport, bool looped)
{
    if (looped && !viewportRequestState(viewport).looping) {
        ++viewportRequestState(viewport).playbackLoopIterationsCompleted;
    }
}

void updateLoopProgressForAcceptedPlaybackTarget(
    ViewportControllerPort& viewport, const PlaybackAdvanceTarget& target)
{
    updateLoopProgressForAcceptedPlaybackTarget(viewport, target.looped);
}

ImageViewport::PlaybackPhase playbackAdvancePhaseForRequest(
    ImageViewport::RequestStatus requestStatus, bool reachedEnd)
{
    if (reachedEnd && requestStatus != ImageViewport::RequestStatus::Loading) {
        return ImageViewport::PlaybackPhase::Stopped;
    }
    return requestStatus == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

}
