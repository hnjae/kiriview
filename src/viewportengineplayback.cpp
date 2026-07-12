#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"
#include "playbacktimeline_p.h"
#include "viewportplaybackcontract_p.h"

#include <algorithm>
#include <utility>

namespace {

void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    const ImageViewportInternal::ViewportChangeSet& source)
{
    target.requestState |= source.requestState;
    target.displayState |= source.displayState;
    target.playbackPhase |= source.playbackPhase;
    target.diagnostics |= source.diagnostics;
    target.displayRevision |= source.displayRevision;
    target.requestRevision |= source.requestRevision;
    target.scheduleUpdate |= source.scheduleUpdate;
}

const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

ImageViewportInternal::RequestState::RoleState& requestRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

bool rolePresent(const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return requestRole(request, role).source.facts.present;
}

void appendCommandChanges(
    const ViewportEngine::CommandResult& command, ImageViewportInternal::ViewportChangeSet& changes)
{
    if (!command.commandRevisionChanged) {
        return;
    }
    changes.commandRevision = true;
    changes.commandRevisionValue
        = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
    changes.diagnostics = true;
}

bool effectiveLooping(
    const ImageViewportInternal::PlaybackState& playback, ImageSequenceAuthoredAnimationFacts facts)
{
    if (playback.looping) {
        return true;
    }
    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return playback.loopIterationsCompleted + 1 < facts.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

}

ViewportEngine::PlaybackCommandResult ViewportEngine::applyPlaybackCommand(
    const PlaybackCommandInput& input)
{
    PlaybackCommandResult result;
    if (!validateViewportPlaybackCommand(input.command)) {
        result.command = rejectInvalidCommand();
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (!rolePresent(m_state->requestState.request, input.command.role)) {
        result.command = rejected(ImageViewport::CommandOutcome::IgnoredNoRequest,
            ImageViewport::CommandReason::IgnoredNoRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        ViewportEnginePlaybackPauseAccess access(m_state->playbackState.playback);
        const auto reduction
            = reduceViewportEnginePlaybackPause({ input.command.role }, std::move(access));
        result.changes.playbackPhase = reduction.playbackPhaseChanged;
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        ViewportEnginePlaybackStopAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackStop(
            { input.command.role, input.geometry }, std::move(access));
        mergeChanges(result.changes, reduction.changes);
        result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
        || input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition) {
        ViewportEnginePlaybackSeekAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackSeek(
            { input.command.kind, input.command.role, input.command.value, input.geometry },
            std::move(access));
        result.command = reduction.outcome == ImageViewport::CommandOutcome::Accepted
            ? accepted()
            : rejected(reduction.outcome, reduction.reason);
        appendCommandChanges(result.command, result.changes);
        mergeChanges(result.changes, reduction.changes);
        result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
        result.schedule = reduction.outcome == ImageViewport::CommandOutcome::Accepted
            ? currentPlaybackSchedule()
            : ViewportPlaybackScheduleEffect {
                  ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    ViewportEnginePlaybackPlayAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction = reduceViewportEnginePlaybackPlay(
        { input.command.role, input.geometry }, std::move(access));
    result.command = reduction.outcome == ImageViewport::CommandOutcome::Accepted
        ? accepted()
        : rejected(reduction.outcome, reduction.reason);
    appendCommandChanges(result.command, result.changes);
    mergeChanges(result.changes, reduction.changes);
    result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
    result.schedule = reduction.outcome == ImageViewport::CommandOutcome::Accepted
        ? currentPlaybackSchedule()
        : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
    return result;
}

void ViewportEngine::setPlaybackPhase(
    ImageViewport::PlaybackPhase phase, ImageViewportInternal::ViewportChangeSet& changes)
{
    if (playbackAccess().playback().phase == phase) {
        return;
    }
    playbackAccess().playback().phase = phase;
    changes.playbackPhase = true;
}

ViewportEngine::PlaybackTickResult ViewportEngine::advancePlayback(const PlaybackTickInput& input)
{
    PlaybackTickResult result;
    if (playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Playing
        || input.elapsedMilliseconds <= 0) {
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }

    const ImageViewport::PageRole role = playbackAccess().playback().role;
    auto& roleRequest = requestRole(playbackAccess().request(), role);
    const auto& source = roleRequest.source;
    auto& provider = playbackAccess().roles()[roleIndex(role)].provider;
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.facts.timingIntervals : source.facts.timingIntervals;
    const auto authoredFacts = providerTiming ? provider.facts.authoredAnimationFacts
                                              : source.facts.authoredAnimationFacts;
    if (!source.facts.present
        || (providerTiming ? (!provider.facts.metadataReady || !provider.facts.timedMetadata
                                 || !provider.facts.timedPlaybackSupport)
                           : (!source.facts.timed || !intervals.isValid()))) {
        result.schedule = currentPlaybackSchedule();
        return result;
    }

    const int currentFrame = roleRequest.activeRequest.target.frame;
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        input.elapsedMilliseconds, currentFrame, playbackAccess().playback().position,
        effectiveLooping(playbackAccess().playback(), authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        result.schedule = currentPlaybackSchedule();
        return result;
    }

    playbackAccess().playback().position = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming && target.displayTarget.frame == currentFrame
        && playbackAccess().request().status == ImageViewport::RequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewport::PageRole::Primary) {
        if (playbackAccess().playback().stopWhenRequestReady || target.reachedEnd) {
            playbackAccess().playback().stopWhenRequestReady = false;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        } else if (target.looped && !playbackAccess().playback().looping) {
            ++playbackAccess().playback().loopIterationsCompleted;
        }
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        result.schedule = currentPlaybackSchedule();
        return result;
    }

    auto displayTarget = target.displayTarget;
    if (providerTiming) {
        displayTarget.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
    }

    if (role == ImageViewport::PageRole::Primary) {
        playbackAccess().request().beginDisplayRequest(
            ImageViewportInternal::DisplayRequestOrigin::Playback, displayTarget,
            { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) }, false);
    } else {
        const auto primaryRequest = playbackAccess().request().roles[0].activeRequest;
        playbackAccess().request().beginDisplayRequest(
            ImageViewportInternal::DisplayRequestOrigin::Playback, primaryRequest.target,
            primaryRequest.resolvedFrame, false);
        playbackAccess().request().roles[1].activeRequest.identity
            = playbackAccess().request().roles[0].activeRequest.identity;
        playbackAccess().request().roles[1].activeRequest.target = displayTarget;
        playbackAccess().request().roles[1].activeRequest.resolvedFrame
            = { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) };
        playbackAccess().request().roles[1].activeRequest.providerFrameToken = {};
        playbackAccess().request().roles[1].activeRequest.preparedPayloadId = 0;
    }
    if (target.looped && !playbackAccess().playback().looping) {
        ++playbackAccess().playback().loopIterationsCompleted;
    }

    if (providerTiming) {
        const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
        auto& effect = result.effects.providerFrameTransport[index];
        auto& active = requestRole(playbackAccess().request(), role).activeRequest;
        bool acceptedDispatch = true;
        if (provider.requests.activeFrameToken.isValid()) {
            const auto queued = queueProviderFrameRequest(
                { role, displayTarget.frame, displayTarget.providerTargetKind });
            effect.cancelToken = queued.cancelToken;
            effect.deferredControllerEvent = queued.deferredFlush
                ? ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest
                : ViewportProviderDeferredControllerEvent::None;
        } else {
            const auto allocation = allocateViewportProviderRequestToken(
                { role }, providerRequestTokenAllocationAccess());
            effect.closeSession = allocation.closeSession;
            effect.sessionClose = allocation.sessionClose;
            mergeChanges(result.changes, allocation.changes);
            if (allocation.exhausted) {
                acceptedDispatch = false;
            } else {
                provider.requests.activeFrameToken = allocation.token;
                active.providerFrameToken = provider.requests.activeFrameToken;
                effect.sendCommand = provider.session.sessionActive;
                effect.command.token = provider.requests.activeFrameToken;
                effect.command.frame = active.resolvedFrame.frame;
                effect.command.position = active.target.position;
                effect.command.targetKind = active.target.providerTargetKind;
                effect.command.demand = providerDisplayDemand(role, input.geometry);
                playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
                playbackAccess().request().reason = ImageViewport::RequestReason::ProviderWaiting;
                playbackAccess().display().status
                    = playbackAccess().display().roles[0].displayedImageSize.isValid()
                    ? ImageViewport::DisplayStatus::Retained
                    : ImageViewport::DisplayStatus::Empty;
                playbackAccess().display().clearPendingRenderPayload();
                playbackAccess().display().clearRenderFailureRetainedDisplay();
            }
        }
        if (acceptedDispatch) {
            playbackAccess().playback().stopWhenRequestReady = target.reachedEnd;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
        }
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.playbackPhase = true;
        result.changes.scheduleUpdate = true;
        result.schedule = currentPlaybackSchedule();
        return result;
    }

    playbackAccess().display().captureRenderFailureRetainedDisplay(true);
    playbackAccess().display().roles[0].pendingRenderPayload.commitPending = true;
    playbackAccess().display().beginPreparedPayloadIdentity(
        playbackAccess().request().sequenceGeneration,
        playbackAccess().request().roles[0].activeRequest);
    const auto primaryAdmission
        = FramePreparation::admitBuiltInFrame(playbackAccess().request().roles[0].source,
            playbackAccess().request().roles[0].activeRequest.target.frame,
            playbackAccess().display().roles[0].pendingRenderPayload);
    playbackAccess().display().roles[0].pendingRenderPayload = primaryAdmission.preparedPayload;
    if (playbackAccess().request().roles[1].source.facts.present
        && !playbackAccess().request().roles[1].source.facts.provider
        && playbackAccess().request().roles[1].activeRequest.target.frame >= 0) {
        ImageViewportInternal::PreparedPayload secondaryPayload;
        secondaryPayload.commitPending = true;
        secondaryPayload.generation = playbackAccess().request().sequenceGeneration;
        secondaryPayload.requestId = playbackAccess().request().roles[0].activeRequest.identity.id;
        secondaryPayload.payloadId = ++playbackAccess().display().nextPreparedPayloadId;
        playbackAccess().request().roles[1].activeRequest.preparedPayloadId
            = secondaryPayload.payloadId;
        const auto secondaryAdmission
            = FramePreparation::admitBuiltInFrame(playbackAccess().request().roles[1].source,
                playbackAccess().request().roles[1].activeRequest.target.frame, secondaryPayload);
        playbackAccess().display().roles[1].pendingRenderPayload
            = secondaryAdmission.preparedPayload;
    }
    playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
    playbackAccess().request().reason = input.geometry.itemBounds.isEmpty()
        ? ImageViewport::RequestReason::RenderWaiting
        : ImageViewport::RequestReason::UploadPending;
    playbackAccess().display().status
        = playbackAccess().display().roles[0].displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    playbackAccess().playback().stopWhenRequestReady = target.reachedEnd;
    playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;

    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.playbackPhase = true;
    result.changes.scheduleUpdate = true;
    result.schedule = currentPlaybackSchedule();
    return result;
}

ViewportPlaybackScheduleEffect ViewportEngine::currentPlaybackSchedule() const
{
    ViewportEnginePlaybackScheduleAccess access(
        m_state->requestState.request, m_state->playbackState.playback, providerFactsView());
    return projectViewportPlaybackSchedule(std::move(access));
}
