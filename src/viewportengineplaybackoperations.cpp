#include "viewportengineplaybackoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <algorithm>
#include <limits>

namespace {
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
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackStopAccess::providerDemand(
    ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry) const
{
    if (m_nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    auto& active = requestRole(m_request, role).activeRequest;
    active.demandRevision
        = ImageViewportInternal::RevisionTokenPrivateAccess::demandFromValue(++m_nextRevision);
    const quint64 presentationRevision
        = m_presentationRevision != 0 ? m_presentationRevision : m_presentationTargetGeneration;
    return projectViewportProviderDemand(
        { role, geometry, active.demandRevision,
            ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(
                m_request.requestRevision),
            ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(
                presentationRevision),
            ImageViewportInternal::RevisionTokenPrivateAccess::generationFromValue(
                m_presentationTargetGeneration) },
        { m_request, m_display,
            { m_roles[0].provider.facts, m_roles[1].provider.facts }, m_presentation });
}

ViewportProviderRequestTokenAllocationResult
ViewportEnginePlaybackStopAccess::allocateProviderRequestToken(ImageViewport::PageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
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

ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
    ViewportEnginePlaybackStopInput input, ViewportEnginePlaybackStopAccess access)
{
    ViewportEnginePlaybackStopReduction result;
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& display = access.m_display;
    const std::size_t index
        = input.role == ImageViewport::PageRole::Secondary ? 1U : 0U;

    if (playback.phase != ImageViewport::PlaybackPhase::Stopped && playback.role != input.role) {
        return result;
    }

    playback.stopWhenRequestReady = false;
    auto& roleState = requestRole(request, input.role);
    const bool providerSource = roleState.source.facts.provider;
    auto& provider = access.m_roles[index].provider;
    if (providerSource && provider.requests.activeFrameToken.isValid()
        && playback.phase != ImageViewport::PlaybackPhase::Stopped
        && playback.role == input.role) {
        result.providerFrameTransport[index].cancelToken = provider.requests.activeFrameToken;
        provider.requests.activeFrameToken = {};
        roleState.activeRequest.providerFrameToken = {};
    }

    auto restore = roleState.latestNonPlaybackRequest;
    if (providerSource && restore.identity.id == 0 && provider.facts.metadataReady) {
        restore = roleState.activeRequest;
        restore.identity.origin = ImageViewportInternal::DisplayRequestOrigin::Initial;
        restore.target.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    if (providerSource && restore.identity.id != 0 && restore.target.frame < 0
        && provider.facts.metadataReady && provider.facts.timedMetadata) {
        restore.target.frame = 0;
        restore.target.position = provider.facts.timingIntervals.frameStartPosition(0);
        restore.target.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Frame;
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

    auto beginRoleRequest = [&request](ImageViewport::PageRole role,
                                ImageViewportInternal::DisplayRequestTarget target,
                                ImageViewportInternal::ResolvedFrameIdentity resolved) {
        if (role == ImageViewport::PageRole::Primary) {
            request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::StopRestore,
                target, resolved, true);
            return;
        }
        const auto primaryRequest = request.roles[0].activeRequest;
        request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::StopRestore,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        auto& secondary = request.roles[1].activeRequest;
        secondary.identity = request.roles[0].activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = resolved;
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = 0;
        request.roles[1].latestNonPlaybackRequest = secondary;
    };
    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto stageBuiltIn = [&request, &display, &input]() {
        request.targetSpreadTerminal.clear();
        request.lastAcceptedRenderFailure = {};
        display.captureRenderFailureRetainedDisplay(true);
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(
            request.sequenceGeneration, request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame,
                display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            ImageViewportInternal::PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = request.sequenceGeneration;
            payload.requestId = request.roles[0].activeRequest.identity.id;
            payload.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
            display.roles[1].pendingRenderPayload
                = FramePreparation::admitBuiltInFrame(request.roles[1].source,
                    request.roles[1].activeRequest.target.frame, payload)
                      .preparedPayload;
        }
        request.status = ImageViewport::RequestStatus::Loading;
        request.reason = input.geometry.itemBounds.isEmpty()
            ? ImageViewport::RequestReason::RenderWaiting
            : ImageViewport::RequestReason::UploadPending;
        display.status = display.roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
    };
    auto dispatchProvider = [&]() {
        auto allocation = access.allocateProviderRequestToken(input.role);
        auto& effect = result.providerFrameTransport[index];
        effect.closeSession = allocation.closeSession;
        effect.sessionClose = allocation.sessionClose;
        mergeChanges(result.changes, allocation.changes);
        if (allocation.exhausted) {
            return;
        }
        provider.requests.activeFrameToken = allocation.token;
        roleState.activeRequest.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = roleState.activeRequest.resolvedFrame.frame;
        effect.command.position = roleState.activeRequest.target.position;
        effect.command.targetKind = roleState.activeRequest.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        request.status = ImageViewport::RequestStatus::Loading;
        request.reason = ImageViewport::RequestReason::ProviderWaiting;
        display.status = display.roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        display.clearPendingRenderPayload();
        display.clearRenderFailureRetainedDisplay();
    };

    if (playback.phase != ImageViewport::PlaybackPhase::Stopped && restore.identity.id != 0
        && (roleState.activeRequest.identity.origin
                == ImageViewportInternal::DisplayRequestOrigin::Playback
            || roleState.activeRequest.target.providerTargetKind
                == ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
        beginRoleRequest(input.role, restore.target, restore.resolvedFrame);
        playback.position = restore.target.position;
        const auto& displayed = display.roles[index].displayedRequest;
        const QSizeF displayedSize = display.roles[index].displayedImageSize;
        if (displayed.generation == request.sequenceGeneration
            && displayed.request.resolvedFrame.frame == restore.resolvedFrame.frame
            && displayed.request.resolvedFrame.position == restore.resolvedFrame.position
            && displayedSize.isValid() && (!providerSource || restore.resolvedFrame.isValid())) {
            request.status = ImageViewport::RequestStatus::Ready;
            request.reason = ImageViewport::RequestReason::Ready;
            display.status = ImageViewport::DisplayStatus::Ready;
            result.changes.requestState = true;
            result.changes.requestRevision = true;
            result.changes.displayState = true;
            result.changes.displayRevision = true;
        } else {
            if (providerSource && restore.target.frame >= 0 && provider.facts.metadataReady) {
                dispatchProvider();
            } else if (providerSource) {
                request.status = ImageViewport::RequestStatus::Loading;
                request.reason = ImageViewport::RequestReason::ProviderWaiting;
                display.status = display.roles[0].displayedImageSize.isValid()
                    ? ImageViewport::DisplayStatus::Retained
                    : ImageViewport::DisplayStatus::Empty;
            } else {
                stageBuiltIn();
            }
            markRequest();
        }
    }
    if (playback.phase != ImageViewport::PlaybackPhase::Stopped) {
        playback.phase = ImageViewport::PlaybackPhase::Stopped;
        result.changes.playbackPhase = true;
    }
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
