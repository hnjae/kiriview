// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineplaybackoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "playbacktimeline_p.h"
#include "viewportenginebuiltinframeoperations_p.h"
#include "viewportenginetargetspreadoperations_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

#include <algorithm>

namespace {
const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewportPageRole role)
{
    return request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}

ImageViewportInternal::RequestState::RoleState& requestRole(
    ImageViewportInternal::RequestState& request, ImageViewportPageRole role)
{
    return request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}

void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    const ImageViewportInternal::ViewportChangeSet& source)
{
    target.requestState |= source.requestState;
    target.displayState |= source.displayState;
    target.playbackPhase |= source.playbackPhase;
    target.diagnostics |= source.diagnostics;
    target.displayRevision |= source.displayRevision;
    target.requestRevision |= source.requestRevision;
    target.commandRevision |= source.commandRevision;
    target.presentationRevision |= source.presentationRevision;
    target.targetPresentationRevision |= source.targetPresentationRevision;
    target.adoptTargetPresentationRevision |= source.adoptTargetPresentationRevision;
    target.scheduleUpdate |= source.scheduleUpdate;
}

bool effectiveLooping(const ImageViewportInternal::PlaybackState& playback,
    ImageViewportPageRole role, ImageSequenceAuthoredAnimationFacts facts)
{
    if (playback.looping) {
        return true;
    }
    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationLoopMode::Unavailable:
        return false;
    case ImageSequenceAuthoredAnimationLoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationLoopMode::Finite:
        return playback.forRole(role).loopIterationsCompleted + 1 < facts.loopCount();
    case ImageSequenceAuthoredAnimationLoopMode::PlayOnce:
        return false;
    }
    return false;
}

bool hasDisplayedPayload(const ImageViewportInternal::DisplayState& display)
{
    return display.roles[0].displayedPayload.hasPresentableContent();
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
    case ImageViewportPageRole::Primary:
    case ImageViewportPageRole::Secondary:
        return true;
    default:
        return false;
    }
}

ViewportEnginePlaybackPauseReduction reduceViewportEnginePlaybackPause(
    ViewportEnginePlaybackPauseInput input, ViewportEnginePlaybackPauseAccess& access)
{
    ViewportEnginePlaybackPauseReduction result;
    auto& playback = access.playback().forRole(input.role);
    if (playback.phase != ImageViewportPlaybackPhase::Playing
        && playback.phase != ImageViewportPlaybackPhase::Waiting) {
        return result;
    }
    playback.phase = ImageViewportPlaybackPhase::Paused;
    result.playbackPhaseChanged = true;
    return result;
}

ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
    ViewportEnginePlaybackStopInput input, ViewportEnginePlaybackStopAccess& access)
{
    ViewportEnginePlaybackStopReduction result;
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& rolePlayback = playback.forRole(input.role);
    auto& display = access.m_display;
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;

    rolePlayback.stopWhenRequestReady = false;
    auto& roleState = requestRole(request, input.role);
    const bool providerSource = roleState.source.facts.provider;
    auto& provider = access.m_roles[index].provider;
    const auto* activeFrame = provider.requests.frameRequest();
    if (providerSource && activeFrame
        && rolePlayback.phase != ImageViewportPlaybackPhase::Stopped) {
        result.providerFrameTransport[index].cancelToken = activeFrame->token;
        provider.requests.retire(activeFrame->token);
    }

    auto restore = roleState.latestNonPlaybackRequest;
    if (providerSource && restore.identity.id == 0 && provider.facts.metadataReady) {
        restore = roleState.activeRequest;
        restore.identity.origin = ImageViewportInternal::DisplayRequestOrigin::Initial;
        restore.target.providerTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    if (providerSource && restore.identity.id != 0 && restore.target.frame < 0
        && provider.facts.metadataReady && provider.facts.timedMetadata) {
        restore.target.frame = 0;
        restore.target.position = provider.facts.timingIntervals.frameStartPosition(0);
        restore.target.providerTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
        restore.resolvedFrame = { 0, provider.facts.timingIntervals.frameStartPosition(0) };
    }
    if (providerSource && restore.target.frame >= 0 && !restore.resolvedFrame.isValid()
        && provider.facts.metadataReady && provider.facts.timedMetadata) {
        const int position
            = provider.facts.timingIntervals.frameStartPosition(restore.target.frame);
        restore.resolvedFrame = { restore.target.frame, position };
        if (restore.target.providerTargetKind
            != ImageViewportInternal::ProviderRequestTargetKind::Position) {
            restore.target.position = position;
        }
    }

    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto stageBuiltIn = [&request, &display, &input, &access, &result]() {
        const auto admission = materializeViewportEngineBuiltInTargetSpread(
            request, access.m_playback, display, access.m_presentation, input.geometry);
        if (!admission.accepted) {
            result.changes.diagnostics = true;
            result.changes.playbackPhase |= admission.playbackStopped;
        }
        return admission;
    };
    auto dispatchProvider = [&]() {
        ViewportEngineProviderRoleMaterializationMutation mutation { request, playback, display,
            access.m_roles, access.m_presentation, access.m_nextRevision,
            access.m_presentationRevision, access.m_presentationTargetGeneration };
        auto materialized = materializeViewportEngineProviderRole(
            { input.role, input.geometry, false }, mutation);
        request = std::move(mutation.request);
        playback = mutation.playback;
        display = std::move(mutation.display);
        access.m_roles = std::move(mutation.roles);
        access.m_nextRevision = mutation.nextRevision;
        const auto cancelled = result.providerFrameTransport[index].cancelToken;
        result.providerFrameTransport[index] = materialized.effect;
        if (!result.providerFrameTransport[index].cancelToken.isValid()) {
            result.providerFrameTransport[index].cancelToken = cancelled;
        }
        mergeChanges(result.changes, materialized.changes);
    };

    if (rolePlayback.phase != ImageViewportPlaybackPhase::Stopped && restore.identity.id != 0
        && (roleState.activeRequest.identity.origin
                == ImageViewportInternal::DisplayRequestOrigin::Playback
            || roleState.activeRequest.target.providerTargetKind
                == ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
        request.beginRoleDisplayRequest(input.role,
            ImageViewportInternal::DisplayRequestOrigin::StopRestore, restore.target,
            restore.resolvedFrame, true);
        rolePlayback.position = restore.target.position;
        const auto& displayed = display.roles[index].displayedRequest;
        const QSizeF displayedSize = display.roles[index].displayedPayload.sourceLogicalSize;
        if (displayed.generation == request.sequenceGeneration
            && displayed.request.resolvedFrame.frame == restore.resolvedFrame.frame
            && displayed.request.resolvedFrame.position == restore.resolvedFrame.position
            && displayedSize.isValid() && (!providerSource || restore.resolvedFrame.isValid())) {
            request.status = ImageViewportRequestStatus::Ready;
            request.reason = ImageViewportRequestReason::Ready;
            display.status = ImageViewportDisplayStatus::Ready;
            result.changes.requestState = true;
            result.changes.requestRevision = true;
            result.changes.displayState = true;
            result.changes.displayRevision = true;
        } else {
            if (providerSource && restore.target.frame >= 0 && provider.facts.metadataReady) {
                dispatchProvider();
            } else if (providerSource) {
                request.status = ImageViewportRequestStatus::Loading;
                request.reason = ImageViewportRequestReason::ProviderWaiting;
                display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                              : ImageViewportDisplayStatus::Empty;
            } else {
                stageBuiltIn();
            }
            markRequest();
        }
    }
    if (rolePlayback.phase != ImageViewportPlaybackPhase::Stopped) {
        rolePlayback.phase = ImageViewportPlaybackPhase::Stopped;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackSeekReduction reduceViewportEnginePlaybackSeek(
    ViewportEnginePlaybackSeekInput input, ViewportEnginePlaybackSeekAccess& access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackSeekReduction result;
    auto reject
        = [&result](ImageViewportCommandOutcome outcome, ImageViewportCommandReason reason) {
              result.outcome = outcome;
              result.reason = reason;
              return result;
          };
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& rolePlayback = playback.forRole(input.role);
    auto& display = access.m_display;
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& roleState = requestRole(request, input.role);
    const auto& source = roleState.source;
    auto& provider = access.m_roles[index].provider;
    const bool positionSeek = input.kind == ViewportPlaybackCommand::Kind::SeekPosition;

    int frame = input.value;
    int position = -1;
    ProviderRequestTargetKind targetKind
        = positionSeek ? ProviderRequestTargetKind::Position : ProviderRequestTargetKind::Frame;
    ResolvedFrameIdentity resolved;

    if (input.value < 0) {
        return reject(
            ImageViewportCommandOutcome::Invalid, ImageViewportCommandReason::InvalidRequest);
    }
    if (source.facts.provider && !provider.facts.metadataReady) {
        const auto capability = positionSeek ? source.facts.providerPositionSeekCapability
                                             : source.facts.providerFrameSeekCapability;
        if (providerCapabilityKnownFalse(capability)) {
            return reject(ImageViewportCommandOutcome::Unsupported,
                ImageViewportCommandReason::UnsupportedRequest);
        }
        frame = positionSeek ? -1 : input.value;
        position = positionSeek ? input.value : -1;
    } else if (source.facts.provider) {
        const bool supported
            = positionSeek ? provider.facts.positionSeekSupport : provider.facts.frameSeekSupport;
        if (!supported) {
            return reject(ImageViewportCommandOutcome::Unsupported,
                ImageViewportCommandReason::UnsupportedRequest);
        }
        const int frameCount
            = provider.facts.timedMetadata ? provider.facts.timingIntervals.frameCount() : 1;
        if (positionSeek) {
            frame = provider.facts.timedMetadata
                ? provider.facts.timingIntervals.frameIndexForPosition(input.value)
                : -1;
            position = input.value;
        } else if (frame >= 0 && frame < frameCount && provider.facts.timedMetadata) {
            position = provider.facts.timingIntervals.frameStartPosition(frame);
        }
        if (frame < 0 || frame >= frameCount) {
            return reject(
                ImageViewportCommandOutcome::Invalid, ImageViewportCommandReason::InvalidRequest);
        }
        resolved = { frame,
            provider.facts.timedMetadata ? provider.facts.timingIntervals.frameStartPosition(frame)
                                         : -1 };
    } else {
        const bool frameSeekOnStill
            = !positionSeek && !source.facts.timed && source.facts.frameCount == 1;
        if (!frameSeekOnStill && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
            return reject(ImageViewportCommandOutcome::Unsupported,
                ImageViewportCommandReason::UnsupportedRequest);
        }
        if (positionSeek) {
            frame = source.facts.timingIntervals.frameIndexForPosition(input.value);
            position = input.value;
        } else if (frame >= 0 && frame < source.facts.frameCount && source.facts.timed) {
            position = source.facts.timingIntervals.frameStartPosition(frame);
        }
        if (frame < 0 || frame >= source.facts.frameCount) {
            return reject(
                ImageViewportCommandOutcome::Invalid, ImageViewportCommandReason::InvalidRequest);
        }
        resolved = { frame,
            source.facts.timed ? source.facts.timingIntervals.frameStartPosition(frame) : -1 };
    }

    if (viewportEngineHasCurrentGenerationTerminal(request)) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }

    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto stageBuiltIn = [&]() {
        const auto admission = materializeViewportEngineBuiltInTargetSpread(
            request, playback, display, access.m_presentation, input.geometry);
        if (!admission.accepted) {
            result.changes.diagnostics = true;
            result.changes.playbackPhase |= admission.playbackStopped;
        }
        return admission;
    };
    auto dispatchProvider = [&]() {
        ViewportEngineProviderRoleMaterializationMutation mutation { request, playback, display,
            access.m_roles, access.m_presentation, access.m_nextRevision,
            access.m_presentationRevision, access.m_presentationTargetGeneration };
        auto materialized = materializeViewportEngineProviderRole(
            { input.role, input.geometry, false }, mutation);
        request = std::move(mutation.request);
        playback = mutation.playback;
        display = std::move(mutation.display);
        access.m_roles = std::move(mutation.roles);
        access.m_nextRevision = mutation.nextRevision;
        result.providerFrameTransport[index] = materialized.effect;
        mergeChanges(result.changes, materialized.changes);
    };

    request.beginRoleDisplayRequest(input.role, DisplayRequestOrigin::ExplicitSeek,
        { frame, position, targetKind }, resolved, true);
    result.changes.diagnostics = request.clearError();
    if (source.facts.provider && !provider.facts.metadataReady) {
        markRequest();
    } else {
        if (source.facts.provider) {
            dispatchProvider();
        } else {
            stageBuiltIn();
        }
        markRequest();
    }
    if (rolePlayback.phase == ImageViewportPlaybackPhase::Playing) {
        rolePlayback.phase = ImageViewportPlaybackPhase::Waiting;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackPlayReduction reduceViewportEnginePlaybackPlay(
    ViewportEnginePlaybackPlayInput input, ViewportEnginePlaybackPlayAccess& access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackPlayReduction result;
    auto reject
        = [&result](ImageViewportCommandOutcome outcome, ImageViewportCommandReason reason) {
              result.outcome = outcome;
              result.reason = reason;
              return result;
          };
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& rolePlayback = playback.forRole(input.role);
    auto& display = access.m_display;
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& roleState = requestRole(request, input.role);
    const auto& source = roleState.source;
    auto& provider = access.m_roles[index].provider;

    if (viewportEngineHasCurrentGenerationTerminal(request)) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }

    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto stageBuiltIn = [&]() {
        const auto admission = materializeViewportEngineBuiltInTargetSpread(
            request, playback, display, access.m_presentation, input.geometry);
        if (!admission.accepted) {
            result.changes.diagnostics = true;
            result.changes.playbackPhase |= admission.playbackStopped;
        }
        return admission;
    };
    auto dispatchProvider = [&]() {
        ViewportEngineProviderRoleMaterializationMutation mutation { request, playback, display,
            access.m_roles, access.m_presentation, access.m_nextRevision,
            access.m_presentationRevision, access.m_presentationTargetGeneration };
        auto materialized
            = materializeViewportEngineProviderRole({ input.role, input.geometry, true }, mutation);
        request = std::move(mutation.request);
        playback = mutation.playback;
        display = std::move(mutation.display);
        access.m_roles = std::move(mutation.roles);
        access.m_nextRevision = mutation.nextRevision;
        result.providerFrameTransport[index] = materialized.effect;
        mergeChanges(result.changes, materialized.changes);
    };

    if (source.facts.provider && !provider.facts.metadataReady) {
        if (providerCapabilityKnownFalse(source.facts.providerTimedPlaybackCapability)) {
            return reject(ImageViewportCommandOutcome::Unsupported,
                ImageViewportCommandReason::UnsupportedRequest);
        }
        rolePlayback.stopWhenRequestReady = false;
        rolePlayback.loopIterationsCompleted = 0;
        if (const auto* activeFrame = provider.requests.frameRequest()) {
            result.providerFrameTransport[index].cancelToken = activeFrame->token;
            provider.requests.retire(activeFrame->token);
        }
        request.beginRoleDisplayRequest(input.role, DisplayRequestOrigin::Playback,
            { -1, -1, ProviderRequestTargetKind::Playback }, { -1, -1 }, false);
        rolePlayback.providerStartPending = true;
        rolePlayback.position = -1;
        rolePlayback.phase = ImageViewportPlaybackPhase::Waiting;
        result.changes.playbackPhase = true;
        markRequest();
        return result;
    }
    if (source.facts.provider
        && (!provider.facts.timedMetadata || !provider.facts.timedPlaybackSupport)) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }
    if (!source.facts.provider
        && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }

    if (!source.facts.provider
        && (request.status == ImageViewportRequestStatus::Unsupported
            || request.status == ImageViewportRequestStatus::Error)) {
        result.changes.diagnostics = request.clearError();
        stageBuiltIn();
        markRequest();
    }
    const bool preservePosition = rolePlayback.position >= 0 && !rolePlayback.stopWhenRequestReady
        && rolePlayback.phase != ImageViewportPlaybackPhase::Stopped;
    rolePlayback.stopWhenRequestReady = false;
    if (!preservePosition) {
        const int frame = roleState.activeRequest.target.frame;
        const auto& intervals
            = source.facts.provider ? provider.facts.timingIntervals : source.facts.timingIntervals;
        rolePlayback.position = intervals.frameStartPosition(frame);
        rolePlayback.loopIterationsCompleted = 0;
    }
    if (source.facts.provider
        && (request.status == ImageViewportRequestStatus::Unsupported
            || request.status == ImageViewportRequestStatus::Error)) {
        auto& active = roleState.activeRequest;
        int frame = active.target.frame;
        if (frame < 0 || frame >= provider.facts.timingIntervals.frameCount()) {
            frame = 0;
        }
        active.target = { frame, provider.facts.timingIntervals.frameStartPosition(frame),
            ProviderRequestTargetKind::Playback };
        active.resolvedFrame = { frame, provider.facts.timingIntervals.frameStartPosition(frame) };
        request.clearError();
        request.targetSpreadTerminal.clear();
        dispatchProvider();
        markRequest();
    }
    const bool terminalRequest = request.status == ImageViewportRequestStatus::Unsupported
        || request.status == ImageViewportRequestStatus::Error;
    const auto phase = terminalRequest ? ImageViewportPlaybackPhase::Stopped
        : request.status == ImageViewportRequestStatus::Loading
        ? ImageViewportPlaybackPhase::Waiting
        : ImageViewportPlaybackPhase::Playing;
    if (rolePlayback.phase != phase) {
        rolePlayback.phase = phase;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackTickReduction reduceViewportEnginePlaybackTick(
    ViewportEnginePlaybackTickInput input, ViewportEnginePlaybackTickAccess& access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackTickReduction result;
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& rolePlayback = playback.forRole(input.role);
    auto& display = access.m_display;
    if (rolePlayback.phase != ImageViewportPlaybackPhase::Playing
        || input.elapsedMilliseconds <= 0) {
        return result;
    }
    result.projectSchedule = true;

    const ImageViewportPageRole role = input.role;
    const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& roleState = requestRole(request, role);
    const auto& source = roleState.source;
    auto& provider = access.m_roles[index].provider;
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.facts.timingIntervals : source.facts.timingIntervals;
    const auto authoredFacts = providerTiming ? provider.facts.authoredAnimationFacts
                                              : source.facts.authoredAnimationFacts;
    if (!source.facts.present
        || (providerTiming ? (!provider.facts.metadataReady || !provider.facts.timedMetadata
                                 || !provider.facts.timedPlaybackSupport)
                           : (!source.facts.timed || !intervals.isValid()))) {
        return result;
    }

    const int currentFrame = roleState.activeRequest.target.frame;
    const auto target = playbackAdvanceTarget(
        input.elapsedMilliseconds, currentFrame, rolePlayback.position,
        effectiveLooping(playback, role, authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    rolePlayback.position = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming && target.displayTarget.frame == currentFrame
        && request.status == ImageViewportRequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewportPageRole::Primary) {
        if (rolePlayback.stopWhenRequestReady || target.reachedEnd) {
            rolePlayback.stopWhenRequestReady = false;
            rolePlayback.phase = ImageViewportPlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        } else if (target.looped && !playback.looping) {
            ++rolePlayback.loopIterationsCompleted;
        }
        return result;
    }
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return result;
    }

    auto displayTarget = target.displayTarget;
    if (providerTiming) {
        displayTarget.providerTargetKind = ProviderRequestTargetKind::Playback;
    }
    request.beginRoleDisplayRequest(role, DisplayRequestOrigin::Playback, displayTarget,
        { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) }, false);
    if (target.looped && !playback.looping) {
        ++rolePlayback.loopIterationsCompleted;
    }

    if (providerTiming) {
        ViewportEngineProviderRoleMaterializationMutation mutation { request, playback, display,
            access.m_roles, access.m_presentation, access.m_nextRevision,
            access.m_presentationRevision, access.m_presentationTargetGeneration };
        auto materialized
            = materializeViewportEngineProviderRole({ role, input.geometry, true }, mutation);
        request = std::move(mutation.request);
        playback = mutation.playback;
        display = std::move(mutation.display);
        access.m_roles = std::move(mutation.roles);
        access.m_nextRevision = mutation.nextRevision;
        result.providerFrameTransport[index] = materialized.effect;
        mergeChanges(result.changes, materialized.changes);
        if (materialized.accepted) {
            auto& mutatedRolePlayback = playback.forRole(role);
            mutatedRolePlayback.stopWhenRequestReady = target.reachedEnd;
            mutatedRolePlayback.phase = ImageViewportPlaybackPhase::Waiting;
        }
    } else {
        const auto admission = materializeViewportEngineBuiltInTargetSpread(
            request, playback, display, access.m_presentation, input.geometry);
        if (admission.accepted) {
            rolePlayback.stopWhenRequestReady = target.reachedEnd;
            rolePlayback.phase = ImageViewportPlaybackPhase::Waiting;
        } else {
            result.changes.diagnostics = true;
            result.changes.playbackPhase |= admission.playbackStopped;
        }
    }

    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.playbackPhase = true;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportEngineAuthoredAutoplayReduction reduceViewportEngineAuthoredAutoplay(
    ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess& access)
{
    using namespace ImageViewportInternal;
    enum class Eligibility {
        Pending,
        Ineligible,
        Eligible,
    };

    ViewportEngineAuthoredAutoplayReduction result;
    auto& playback = access.playback();
    if (access.requestStatus() == ImageViewportRequestStatus::NoRequest
        || access.requestStatus() == ImageViewportRequestStatus::Unsupported
        || access.requestStatus() == ImageViewportRequestStatus::Error) {
        for (auto& rolePlayback : playback.roles) {
            if (rolePlayback.authoredAutoplayArbitration
                == AuthoredAutoplayArbitrationState::Pending) {
                rolePlayback.authoredAutoplayArbitration
                    = AuthoredAutoplayArbitrationState::Resolved;
                result.resolved = true;
            }
        }
        return result;
    }

    const auto eligibility = [&access](ImageViewportPageRole role) {
        const auto& source = access.source(role);
        if (!source.facts.present) {
            return Eligibility::Ineligible;
        }
        if (!source.facts.provider) {
            const auto& request = access.activeRequest(role);
            const bool validTarget = request.target.frame >= 0
                && request.target.frame < source.facts.timingIntervals.frameCount();
            return source.facts.timed && source.facts.timingIntervals.isValid()
                    && source.facts.authoredAnimationFactsAvailable
                    && source.facts.authoredAnimationFacts.autoplay() && validTarget
                ? Eligibility::Eligible
                : Eligibility::Ineligible;
        }

        const auto& provider = access.providerFacts(role);
        if (!provider.metadataReady) {
            if (providerCapabilityKnownFalse(source.facts.providerTimedPlaybackCapability)
                || (provider.authoredAnimationFactsAvailable
                    && !provider.authoredAnimationFacts.autoplay())) {
                return Eligibility::Ineligible;
            }
            return Eligibility::Pending;
        }
        const auto& request = access.activeRequest(role);
        const bool validTarget = request.target.frame >= 0
            && request.target.frame < provider.timingIntervals.frameCount();
        return provider.timedMetadata && provider.timedPlaybackSupport
                && provider.authoredAnimationFactsAvailable
                && provider.authoredAnimationFacts.autoplay() && validTarget
            ? Eligibility::Eligible
            : Eligibility::Ineligible;
    };

    for (const auto role : { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        auto& rolePlayback = playback.forRole(role);
        if (rolePlayback.authoredAutoplayArbitration != AuthoredAutoplayArbitrationState::Pending) {
            continue;
        }
        const Eligibility roleEligibility = eligibility(role);
        if (roleEligibility == Eligibility::Pending) {
            continue;
        }
        rolePlayback.authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Resolved;
        result.resolved = true;
        if (roleEligibility != Eligibility::Eligible) {
            continue;
        }
        const auto& source = access.source(role);
        const auto& intervals = source.facts.provider ? access.providerFacts(role).timingIntervals
                                                      : source.facts.timingIntervals;
        const auto previousPhase = rolePlayback.phase;
        rolePlayback.position
            = intervals.frameStartPosition(access.activeRequest(role).target.frame);
        rolePlayback.stopWhenRequestReady = false;
        rolePlayback.providerStartPending = false;
        rolePlayback.loopIterationsCompleted = 0;
        rolePlayback.phase = access.requestStatus() == ImageViewportRequestStatus::Ready
            ? ImageViewportPlaybackPhase::Playing
            : ImageViewportPlaybackPhase::Waiting;
        result.armed = true;
        result.playbackPhaseChanged
            = result.playbackPhaseChanged || previousPhase != rolePlayback.phase;
    }
    result.resolved = std::ranges::all_of(playback.roles, [](const auto& rolePlayback) {
        return rolePlayback.authoredAutoplayArbitration
            != AuthoredAutoplayArbitrationState::Pending;
    });
    return result;
}

ViewportPlaybackScheduleEffect projectViewportPlaybackSchedule(
    ViewportEnginePlaybackScheduleAccess access, ImageViewportPageRole role)
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    if (access.playback().forRole(role).phase != ImageViewportPlaybackPhase::Playing) {
        return { Action::Stop, -1, role };
    }
    const auto& roleRequest = requestRole(access.request(), role);
    const auto& source = roleRequest.source;
    const auto& provider
        = access.providerFacts()[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.timingIntervals : source.facts.timingIntervals;
    const int frameCount = providerTiming ? intervals.frameCount() : source.facts.frameCount;
    const int totalDuration
        = providerTiming ? intervals.totalDuration() : source.facts.totalDuration;
    if (providerTiming && (!provider.metadataReady || !provider.timedMetadata)) {
        return { Action::Stop, -1, role };
    }

    const int currentFrame = roleRequest.activeRequest.target.frame;
    if (currentFrame < 0 || currentFrame >= frameCount) {
        return { Action::Stop, -1, role };
    }

    const int frameStart = intervals.frameStartPosition(currentFrame);
    const int nextFrameStart = currentFrame + 1 < frameCount
        ? intervals.frameStartPosition(currentFrame + 1)
        : totalDuration;
    const int frameDuration = nextFrameStart - frameStart;
    if (frameStart < 0 || frameDuration <= 0) {
        return { Action::Stop, -1, role };
    }

    const int playbackPosition = access.playback().forRole(role).position >= 0
        ? access.playback().forRole(role).position
        : frameStart;
    return { Action::ArmAfter, std::max(1, frameStart + frameDuration - playbackPosition), role };
}
