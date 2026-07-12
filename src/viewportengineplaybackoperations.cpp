#include "viewportengineplaybackoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"
#include "playbacktimeline_p.h"

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

bool effectiveLooping(const ImageViewportInternal::PlaybackState& playback,
    ImageSequenceAuthoredAnimationFacts facts)
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

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackSeekAccess::providerDemand(
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
ViewportEnginePlaybackSeekAccess::allocateProviderRequestToken(ImageViewport::PageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackPlayAccess::providerDemand(
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
ViewportEnginePlaybackPlayAccess::allocateProviderRequestToken(ImageViewport::PageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackTickAccess::providerDemand(
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
ViewportEnginePlaybackTickAccess::allocateProviderRequestToken(ImageViewport::PageRole role)
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

ViewportEnginePlaybackSeekReduction reduceViewportEnginePlaybackSeek(
    ViewportEnginePlaybackSeekInput input, ViewportEnginePlaybackSeekAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackSeekReduction result;
    auto reject = [&result](ImageViewport::CommandOutcome outcome,
                      ImageViewport::CommandReason reason) {
        result.outcome = outcome;
        result.reason = reason;
        return result;
    };
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& display = access.m_display;
    const std::size_t index
        = input.role == ImageViewport::PageRole::Secondary ? 1U : 0U;
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
        return reject(ImageViewport::CommandOutcome::Invalid,
            ImageViewport::CommandReason::InvalidRequest);
    }
    if (source.facts.provider && !provider.facts.metadataReady) {
        const auto capability = positionSeek ? source.facts.providerPositionSeekCapability
                                             : source.facts.providerFrameSeekCapability;
        if (providerCapabilityKnownFalse(capability)) {
            return reject(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
        }
        frame = positionSeek ? -1 : input.value;
        position = positionSeek ? input.value : -1;
    } else if (source.facts.provider) {
        const bool supported
            = positionSeek ? provider.facts.positionSeekSupport : provider.facts.frameSeekSupport;
        if (!supported) {
            return reject(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
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
            return reject(ImageViewport::CommandOutcome::Invalid,
                ImageViewport::CommandReason::InvalidRequest);
        }
        resolved = { frame,
            provider.facts.timedMetadata
                ? provider.facts.timingIntervals.frameStartPosition(frame)
                : -1 };
    } else {
        const bool frameSeekOnStill
            = !positionSeek && !source.facts.timed && source.facts.frameCount == 1;
        if (!frameSeekOnStill
            && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
            return reject(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
        }
        if (positionSeek) {
            frame = source.facts.timingIntervals.frameIndexForPosition(input.value);
            position = input.value;
        } else if (frame >= 0 && frame < source.facts.frameCount && source.facts.timed) {
            position = source.facts.timingIntervals.frameStartPosition(frame);
        }
        if (frame < 0 || frame >= source.facts.frameCount) {
            return reject(ImageViewport::CommandOutcome::Invalid,
                ImageViewport::CommandReason::InvalidRequest);
        }
        resolved = { frame,
            source.facts.timed ? source.facts.timingIntervals.frameStartPosition(frame) : -1 };
    }

    const auto& terminal = request.targetSpreadTerminal;
    const bool generationTerminal = terminal.sealed
        && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id
        && ((terminal.primary.terminal
                && terminal.primary.failureScope == FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope == FailureScope::Generation));
    const bool providerTransportUnavailable = source.facts.provider
        && !provider.session.sessionActive
        && (request.status == ImageViewport::RequestStatus::Unsupported
            || request.status == ImageViewport::RequestStatus::Error);
    if (generationTerminal || providerTransportUnavailable) {
        return reject(ImageViewport::CommandOutcome::Unsupported,
            ImageViewport::CommandReason::UnsupportedRequest);
    }

    auto beginRoleRequest = [&request](ImageViewport::PageRole role,
                                DisplayRequestTarget target, ResolvedFrameIdentity identity) {
        if (role == ImageViewport::PageRole::Primary) {
            request.beginDisplayRequest(
                DisplayRequestOrigin::ExplicitSeek, target, identity, true);
            return;
        }
        const auto primary = request.roles[0].activeRequest;
        request.beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, primary.target,
            primary.resolvedFrame, false);
        auto& secondary = request.roles[1].activeRequest;
        secondary.identity = request.roles[0].activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = identity;
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
    auto stageBuiltIn = [&]() {
        request.targetSpreadTerminal.clear();
        request.lastAcceptedRenderFailure = {};
        display.captureRenderFailureRetainedDisplay(true);
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(request.sequenceGeneration,
            request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame,
                display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            PreparedPayload payload;
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
        auto& effect = result.providerFrameTransport[index];
        auto& active = roleState.activeRequest;
        if (provider.requests.activeFrameToken.isValid()) {
            TargetSpreadWaitState wait;
            if (input.role == ImageViewport::PageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewport::RequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewport::DisplayStatus::Ready
                                      || display.status == ImageViewport::DisplayStatus::Retained)
                && display.roles[0].displayedImageSize.isValid();
            display.status = retained ? ImageViewport::DisplayStatus::Retained
                                      : ImageViewport::DisplayStatus::Empty;
            display.clearPendingRenderPayload();
            display.clearRenderFailureRetainedDisplay();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = false;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredControllerEvent
                = ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest;
            return;
        }
        auto allocation = access.allocateProviderRequestToken(input.role);
        effect.closeSession = allocation.closeSession;
        effect.sessionClose = allocation.sessionClose;
        mergeChanges(result.changes, allocation.changes);
        if (allocation.exhausted) {
            return;
        }
        provider.requests.activeFrameToken = allocation.token;
        active.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        request.status = ImageViewport::RequestStatus::Loading;
        request.reason = ImageViewport::RequestReason::ProviderWaiting;
        display.status = display.roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        display.clearPendingRenderPayload();
        display.clearRenderFailureRetainedDisplay();
    };

    beginRoleRequest(input.role, { frame, position, targetKind }, resolved);
    if (source.facts.provider && !provider.facts.metadataReady) {
        markRequest();
    } else {
        result.changes.diagnostics = request.clearDiagnostics();
        if (source.facts.provider) {
            dispatchProvider();
        } else {
            stageBuiltIn();
        }
        markRequest();
    }
    if (playback.phase == ImageViewport::PlaybackPhase::Playing) {
        playback.phase = ImageViewport::PlaybackPhase::Waiting;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackPlayReduction reduceViewportEnginePlaybackPlay(
    ViewportEnginePlaybackPlayInput input, ViewportEnginePlaybackPlayAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackPlayReduction result;
    auto reject = [&result](ImageViewport::CommandOutcome outcome,
                      ImageViewport::CommandReason reason) {
        result.outcome = outcome;
        result.reason = reason;
        return result;
    };
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& display = access.m_display;
    const std::size_t index
        = input.role == ImageViewport::PageRole::Secondary ? 1U : 0U;
    auto& roleState = requestRole(request, input.role);
    const auto& source = roleState.source;
    auto& provider = access.m_roles[index].provider;

    const auto& terminal = request.targetSpreadTerminal;
    const bool generationTerminal = terminal.sealed
        && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id
        && ((terminal.primary.terminal
                && terminal.primary.failureScope == FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope == FailureScope::Generation));
    const bool providerTransportUnavailable = source.facts.provider
        && !provider.session.sessionActive
        && (request.status == ImageViewport::RequestStatus::Unsupported
            || request.status == ImageViewport::RequestStatus::Error);
    if (generationTerminal || providerTransportUnavailable) {
        return reject(ImageViewport::CommandOutcome::Unsupported,
            ImageViewport::CommandReason::UnsupportedRequest);
    }

    auto beginRoleRequest = [&request](ImageViewport::PageRole role,
                                DisplayRequestOrigin origin, DisplayRequestTarget target,
                                ResolvedFrameIdentity resolved, bool remember) {
        if (role == ImageViewport::PageRole::Primary) {
            request.beginDisplayRequest(origin, target, resolved, remember);
            return;
        }
        const auto primary = request.roles[0].activeRequest;
        request.beginDisplayRequest(origin, primary.target, primary.resolvedFrame, false);
        auto& secondary = request.roles[1].activeRequest;
        secondary.identity = request.roles[0].activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = resolved;
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = 0;
        if (remember) {
            request.roles[1].latestNonPlaybackRequest = secondary;
        }
    };
    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto stageBuiltIn = [&]() {
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
            PreparedPayload payload;
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
        auto& effect = result.providerFrameTransport[index];
        auto& active = roleState.activeRequest;
        if (provider.requests.activeFrameToken.isValid()) {
            TargetSpreadWaitState wait;
            if (input.role == ImageViewport::PageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewport::RequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewport::DisplayStatus::Ready
                                      || display.status == ImageViewport::DisplayStatus::Retained)
                && display.roles[0].displayedImageSize.isValid();
            display.status = retained ? ImageViewport::DisplayStatus::Retained
                                      : ImageViewport::DisplayStatus::Empty;
            display.clearPendingRenderPayload();
            display.clearRenderFailureRetainedDisplay();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = true;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredControllerEvent
                = ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest;
            return;
        }
        auto allocation = access.allocateProviderRequestToken(input.role);
        effect.closeSession = allocation.closeSession;
        effect.sessionClose = allocation.sessionClose;
        mergeChanges(result.changes, allocation.changes);
        if (allocation.exhausted) {
            return;
        }
        provider.requests.activeFrameToken = allocation.token;
        active.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        request.status = ImageViewport::RequestStatus::Loading;
        request.reason = ImageViewport::RequestReason::ProviderWaiting;
        display.status = display.roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        display.clearPendingRenderPayload();
        display.clearRenderFailureRetainedDisplay();
    };

    if (source.facts.provider && !provider.facts.metadataReady) {
        if (providerCapabilityKnownFalse(source.facts.providerTimedPlaybackCapability)) {
            return reject(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
        }
        playback.role = input.role;
        playback.stopWhenRequestReady = false;
        playback.loopIterationsCompleted = 0;
        if (provider.requests.activeFrameToken.isValid()) {
            result.providerFrameTransport[index].cancelToken
                = provider.requests.activeFrameToken;
            provider.requests.activeFrameToken = {};
        }
        if (input.role == ImageViewport::PageRole::Primary) {
            beginRoleRequest(input.role, DisplayRequestOrigin::Playback,
                { -1, -1, ProviderRequestTargetKind::Playback }, { -1, -1 }, false);
        } else {
            auto& secondary = request.roles[1].activeRequest;
            secondary.target = { -1, -1, ProviderRequestTargetKind::Playback };
            secondary.resolvedFrame = { -1, -1 };
            secondary.providerFrameToken = {};
        }
        playback.providerStartPending = input.role == ImageViewport::PageRole::Primary;
        playback.position = -1;
        playback.phase = ImageViewport::PlaybackPhase::Waiting;
        result.changes.playbackPhase = true;
        markRequest();
        return result;
    }
    if (source.facts.provider
        && (!provider.facts.timedMetadata || !provider.facts.timedPlaybackSupport)) {
        return reject(ImageViewport::CommandOutcome::Unsupported,
            ImageViewport::CommandReason::UnsupportedRequest);
    }
    if (!source.facts.provider
        && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
        return reject(ImageViewport::CommandOutcome::Unsupported,
            ImageViewport::CommandReason::UnsupportedRequest);
    }

    if (!source.facts.provider
        && (request.status == ImageViewport::RequestStatus::Unsupported
            || request.status == ImageViewport::RequestStatus::Error)) {
        result.changes.diagnostics = request.clearDiagnostics();
        stageBuiltIn();
        markRequest();
    }
    const bool preservePosition = playback.role == input.role && playback.position >= 0
        && !playback.stopWhenRequestReady
        && playback.phase != ImageViewport::PlaybackPhase::Stopped;
    playback.role = input.role;
    playback.stopWhenRequestReady = false;
    if (!preservePosition) {
        const int frame = roleState.activeRequest.target.frame;
        const auto& intervals
            = source.facts.provider ? provider.facts.timingIntervals : source.facts.timingIntervals;
        playback.position = intervals.frameStartPosition(frame);
        playback.loopIterationsCompleted = 0;
    }
    if (source.facts.provider
        && (request.status == ImageViewport::RequestStatus::Unsupported
            || request.status == ImageViewport::RequestStatus::Error)) {
        auto& active = roleState.activeRequest;
        int frame = active.target.frame;
        if (frame < 0 || frame >= provider.facts.timingIntervals.frameCount()) {
            frame = 0;
        }
        active.target = { frame, provider.facts.timingIntervals.frameStartPosition(frame),
            ProviderRequestTargetKind::Playback };
        active.resolvedFrame = { frame, provider.facts.timingIntervals.frameStartPosition(frame) };
        request.clearDiagnostics();
        request.targetSpreadTerminal.clear();
        dispatchProvider();
        markRequest();
    }
    const auto phase = request.status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
    if (playback.phase != phase) {
        playback.phase = phase;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackTickReduction reduceViewportEnginePlaybackTick(
    ViewportEnginePlaybackTickInput input, ViewportEnginePlaybackTickAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEnginePlaybackTickReduction result;
    auto& request = access.m_request;
    auto& playback = access.m_playback;
    auto& display = access.m_display;
    if (playback.phase != ImageViewport::PlaybackPhase::Playing
        || input.elapsedMilliseconds <= 0) {
        return result;
    }
    result.projectSchedule = true;

    const ImageViewport::PageRole role = playback.role;
    const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
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
    const auto target = playbackAdvanceTarget(input.elapsedMilliseconds, currentFrame,
        playback.position, effectiveLooping(playback, authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    playback.position = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming
        && target.displayTarget.frame == currentFrame
        && request.status == ImageViewport::RequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewport::PageRole::Primary) {
        if (playback.stopWhenRequestReady || target.reachedEnd) {
            playback.stopWhenRequestReady = false;
            playback.phase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        } else if (target.looped && !playback.looping) {
            ++playback.loopIterationsCompleted;
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
    if (role == ImageViewport::PageRole::Primary) {
        request.beginDisplayRequest(DisplayRequestOrigin::Playback, displayTarget,
            { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) }, false);
    } else {
        const auto primary = request.roles[0].activeRequest;
        request.beginDisplayRequest(
            DisplayRequestOrigin::Playback, primary.target, primary.resolvedFrame, false);
        auto& secondary = request.roles[1].activeRequest;
        secondary.identity = request.roles[0].activeRequest.identity;
        secondary.target = displayTarget;
        secondary.resolvedFrame
            = { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) };
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = 0;
    }
    if (target.looped && !playback.looping) {
        ++playback.loopIterationsCompleted;
    }

    if (providerTiming) {
        auto& effect = result.providerFrameTransport[index];
        auto& active = roleState.activeRequest;
        bool acceptedDispatch = true;
        if (provider.requests.activeFrameToken.isValid()) {
            TargetSpreadWaitState wait;
            if (role == ImageViewport::PageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewport::RequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewport::DisplayStatus::Ready
                                      || display.status == ImageViewport::DisplayStatus::Retained)
                && display.roles[0].displayedImageSize.isValid();
            display.status = retained ? ImageViewport::DisplayStatus::Retained
                                      : ImageViewport::DisplayStatus::Empty;
            display.clearPendingRenderPayload();
            display.clearRenderFailureRetainedDisplay();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = true;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredControllerEvent
                = ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest;
        } else {
            auto allocation = access.allocateProviderRequestToken(role);
            effect.closeSession = allocation.closeSession;
            effect.sessionClose = allocation.sessionClose;
            mergeChanges(result.changes, allocation.changes);
            if (allocation.exhausted) {
                acceptedDispatch = false;
            } else {
                provider.requests.activeFrameToken = allocation.token;
                active.providerFrameToken = allocation.token;
                effect.sendCommand = provider.session.sessionActive;
                effect.command.token = allocation.token;
                effect.command.frame = active.resolvedFrame.frame;
                effect.command.position = active.target.position;
                effect.command.targetKind = active.target.providerTargetKind;
                effect.command.demand = access.providerDemand(role, input.geometry);
                request.status = ImageViewport::RequestStatus::Loading;
                request.reason = ImageViewport::RequestReason::ProviderWaiting;
                display.status = display.roles[0].displayedImageSize.isValid()
                    ? ImageViewport::DisplayStatus::Retained
                    : ImageViewport::DisplayStatus::Empty;
                display.clearPendingRenderPayload();
                display.clearRenderFailureRetainedDisplay();
            }
        }
        if (acceptedDispatch) {
            playback.stopWhenRequestReady = target.reachedEnd;
            playback.phase = ImageViewport::PlaybackPhase::Waiting;
        }
    } else {
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
            PreparedPayload secondary;
            secondary.commitPending = true;
            secondary.generation = request.sequenceGeneration;
            secondary.requestId = request.roles[0].activeRequest.identity.id;
            secondary.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = secondary.payloadId;
            display.roles[1].pendingRenderPayload
                = FramePreparation::admitBuiltInFrame(request.roles[1].source,
                    request.roles[1].activeRequest.target.frame, secondary)
                      .preparedPayload;
        }
        request.status = ImageViewport::RequestStatus::Loading;
        request.reason = input.geometry.itemBounds.isEmpty()
            ? ImageViewport::RequestReason::RenderWaiting
            : ImageViewport::RequestReason::UploadPending;
        display.status = display.roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        playback.stopWhenRequestReady = target.reachedEnd;
        playback.phase = ImageViewport::PlaybackPhase::Waiting;
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
