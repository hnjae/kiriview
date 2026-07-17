#include "viewportengineplaybackoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"
#include "playbacktimeline_p.h"

#include <algorithm>
#include <limits>

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

bool effectiveLooping(
    const ImageViewportInternal::PlaybackState& playback, ImageSequenceAuthoredAnimationFacts facts)
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
        return playback.loopIterationsCompleted + 1 < facts.loopCount();
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

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackStopAccess::providerDemand(
    ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) const
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
        { m_request, m_display, { m_roles[0].provider.facts, m_roles[1].provider.facts },
            m_presentation });
}

ViewportProviderRequestTokenAllocationResult
ViewportEnginePlaybackStopAccess::allocateProviderRequestToken(ImageViewportPageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackSeekAccess::providerDemand(
    ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) const
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
        { m_request, m_display, { m_roles[0].provider.facts, m_roles[1].provider.facts },
            m_presentation });
}

ViewportProviderRequestTokenAllocationResult
ViewportEnginePlaybackSeekAccess::allocateProviderRequestToken(ImageViewportPageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackPlayAccess::providerDemand(
    ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) const
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
        { m_request, m_display, { m_roles[0].provider.facts, m_roles[1].provider.facts },
            m_presentation });
}

ViewportProviderRequestTokenAllocationResult
ViewportEnginePlaybackPlayAccess::allocateProviderRequestToken(ImageViewportPageRole role)
{
    return allocateViewportProviderRequestToken(
        { role }, { m_roles, m_request, m_playback, m_display });
}

ImageSequenceProviderDisplayDemand ViewportEnginePlaybackTickAccess::providerDemand(
    ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) const
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
        { m_request, m_display, { m_roles[0].provider.facts, m_roles[1].provider.facts },
            m_presentation });
}

ViewportProviderRequestTokenAllocationResult
ViewportEnginePlaybackTickAccess::allocateProviderRequestToken(ImageViewportPageRole role)
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
    case ImageViewportPageRole::Primary:
    case ImageViewportPageRole::Secondary:
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
        || (access.playback().phase != ImageViewportPlaybackPhase::Playing
            && access.playback().phase != ImageViewportPlaybackPhase::Waiting)) {
        return result;
    }
    access.playback().phase = ImageViewportPlaybackPhase::Paused;
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
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;

    if (playback.phase != ImageViewportPlaybackPhase::Stopped && playback.role != input.role) {
        return result;
    }

    playback.stopWhenRequestReady = false;
    auto& roleState = requestRole(request, input.role);
    const bool providerSource = roleState.source.facts.provider;
    auto& provider = access.m_roles[index].provider;
    if (providerSource && provider.requests.activeFrameToken.isValid()
        && playback.phase != ImageViewportPlaybackPhase::Stopped && playback.role == input.role) {
        result.providerFrameTransport[index].cancelToken = provider.requests.activeFrameToken;
        provider.requests.activeFrameToken = {};
        provider.requests.activeFrameRefinement = false;
        roleState.activeRequest.providerFrameToken = {};
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

    auto beginRoleRequest = [&request](ImageViewportPageRole role,
                                ImageViewportInternal::DisplayRequestTarget target,
                                ImageViewportInternal::ResolvedFrameIdentity resolved) {
        if (role == ImageViewportPageRole::Primary) {
            request.beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, target, resolved, true);
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
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(
            request.sequenceGeneration, request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            ImageViewportInternal::PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = request.sequenceGeneration;
            payload.requestId = request.roles[0].activeRequest.identity.id;
            payload.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
            display.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
                request.roles[1].source, request.roles[1].activeRequest.target.frame, payload)
                                                        .preparedPayload;
        }
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty())
            ? ImageViewportRequestReason::RenderWaiting
            : ImageViewportRequestReason::UploadPending;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
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
        provider.requests.activeFrameRefinement = false;
        roleState.activeRequest.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = roleState.activeRequest.resolvedFrame.frame;
        effect.command.position = roleState.activeRequest.target.position;
        effect.command.targetKind = roleState.activeRequest.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        provider.requests.lastFrameDemand = effect.command.demand;
        provider.requests.hasLastFrameDemand = true;
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = ImageViewportRequestReason::ProviderWaiting;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
        display.clearPendingRenderPayload();
    };

    if (playback.phase != ImageViewportPlaybackPhase::Stopped && restore.identity.id != 0
        && (roleState.activeRequest.identity.origin
                == ImageViewportInternal::DisplayRequestOrigin::Playback
            || roleState.activeRequest.target.providerTargetKind
                == ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
        beginRoleRequest(input.role, restore.target, restore.resolvedFrame);
        playback.position = restore.target.position;
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
    if (playback.phase != ImageViewportPlaybackPhase::Stopped) {
        playback.phase = ImageViewportPlaybackPhase::Stopped;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackSeekReduction reduceViewportEnginePlaybackSeek(
    ViewportEnginePlaybackSeekInput input, ViewportEnginePlaybackSeekAccess access)
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

    const auto& terminal = request.targetSpreadTerminal;
    const bool generationTerminal = terminal.sealed
        && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id
        && ((terminal.primary.terminal && terminal.primary.failureScope == FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope == FailureScope::Generation));
    const bool providerTransportUnavailable = source.facts.provider
        && !provider.session.sessionActive
        && (request.status == ImageViewportRequestStatus::Unsupported
            || request.status == ImageViewportRequestStatus::Error);
    if (generationTerminal || providerTransportUnavailable) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }

    auto beginRoleRequest = [&request](ImageViewportPageRole role, DisplayRequestTarget target,
                                ResolvedFrameIdentity identity) {
        if (role == ImageViewportPageRole::Primary) {
            request.beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, target, identity, true);
            return;
        }
        const auto primary = request.roles[0].activeRequest;
        request.beginDisplayRequest(
            DisplayRequestOrigin::ExplicitSeek, primary.target, primary.resolvedFrame, false);
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
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(
            request.sequenceGeneration, request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = request.sequenceGeneration;
            payload.requestId = request.roles[0].activeRequest.identity.id;
            payload.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
            display.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
                request.roles[1].source, request.roles[1].activeRequest.target.frame, payload)
                                                        .preparedPayload;
        }
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty())
            ? ImageViewportRequestReason::RenderWaiting
            : ImageViewportRequestReason::UploadPending;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
    };
    auto dispatchProvider = [&]() {
        auto& effect = result.providerFrameTransport[index];
        auto& active = roleState.activeRequest;
        if (provider.requests.activeFrameToken.isValid()
            && provider.requests.activeFrameRefinement) {
            effect.cancelToken = provider.requests.activeFrameToken;
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
            active.providerFrameToken = {};
        }
        if (provider.requests.activeFrameToken.isValid()) {
            TargetSpreadWaitState wait;
            if (input.role == ImageViewportPageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewportRequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewportDisplayStatus::Ready
                                      || display.status == ImageViewportDisplayStatus::Retained)
                && hasDisplayedPayload(display);
            display.status = retained ? ImageViewportDisplayStatus::Retained
                                      : ImageViewportDisplayStatus::Empty;
            display.clearPendingRenderPayload();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = false;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredEngineEvent
                = ViewportProviderDeferredEngineEvent::FlushQueuedFrameRequest;
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
        provider.requests.activeFrameRefinement = false;
        active.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        provider.requests.lastFrameDemand = effect.command.demand;
        provider.requests.hasLastFrameDemand = true;
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = ImageViewportRequestReason::ProviderWaiting;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
        display.clearPendingRenderPayload();
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
    if (playback.phase == ImageViewportPlaybackPhase::Playing) {
        playback.phase = ImageViewportPlaybackPhase::Waiting;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportEnginePlaybackPlayReduction reduceViewportEnginePlaybackPlay(
    ViewportEnginePlaybackPlayInput input, ViewportEnginePlaybackPlayAccess access)
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
    auto& display = access.m_display;
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& roleState = requestRole(request, input.role);
    const auto& source = roleState.source;
    auto& provider = access.m_roles[index].provider;

    const auto& terminal = request.targetSpreadTerminal;
    const bool generationTerminal = terminal.sealed
        && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id
        && ((terminal.primary.terminal && terminal.primary.failureScope == FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope == FailureScope::Generation));
    const bool providerTransportUnavailable = source.facts.provider
        && !provider.session.sessionActive
        && (request.status == ImageViewportRequestStatus::Unsupported
            || request.status == ImageViewportRequestStatus::Error);
    if (generationTerminal || providerTransportUnavailable) {
        return reject(ImageViewportCommandOutcome::Unsupported,
            ImageViewportCommandReason::UnsupportedRequest);
    }

    auto beginRoleRequest
        = [&request](ImageViewportPageRole role, DisplayRequestOrigin origin,
              DisplayRequestTarget target, ResolvedFrameIdentity resolved, bool remember) {
              if (role == ImageViewportPageRole::Primary) {
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
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(
            request.sequenceGeneration, request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = request.sequenceGeneration;
            payload.requestId = request.roles[0].activeRequest.identity.id;
            payload.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
            display.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
                request.roles[1].source, request.roles[1].activeRequest.target.frame, payload)
                                                        .preparedPayload;
        }
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty())
            ? ImageViewportRequestReason::RenderWaiting
            : ImageViewportRequestReason::UploadPending;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
    };
    auto dispatchProvider = [&]() {
        auto& effect = result.providerFrameTransport[index];
        auto& active = roleState.activeRequest;
        if (provider.requests.activeFrameToken.isValid()
            && provider.requests.activeFrameRefinement) {
            effect.cancelToken = provider.requests.activeFrameToken;
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
            active.providerFrameToken = {};
        }
        if (provider.requests.activeFrameToken.isValid()) {
            TargetSpreadWaitState wait;
            if (input.role == ImageViewportPageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewportRequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewportDisplayStatus::Ready
                                      || display.status == ImageViewportDisplayStatus::Retained)
                && hasDisplayedPayload(display);
            display.status = retained ? ImageViewportDisplayStatus::Retained
                                      : ImageViewportDisplayStatus::Empty;
            display.clearPendingRenderPayload();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = true;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredEngineEvent
                = ViewportProviderDeferredEngineEvent::FlushQueuedFrameRequest;
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
        provider.requests.activeFrameRefinement = false;
        active.providerFrameToken = allocation.token;
        effect.sendCommand = provider.session.sessionActive;
        effect.command.token = allocation.token;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        effect.command.demand = access.providerDemand(input.role, input.geometry);
        provider.requests.lastFrameDemand = effect.command.demand;
        provider.requests.hasLastFrameDemand = true;
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = ImageViewportRequestReason::ProviderWaiting;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
        display.clearPendingRenderPayload();
    };

    if (source.facts.provider && !provider.facts.metadataReady) {
        if (providerCapabilityKnownFalse(source.facts.providerTimedPlaybackCapability)) {
            return reject(ImageViewportCommandOutcome::Unsupported,
                ImageViewportCommandReason::UnsupportedRequest);
        }
        playback.role = input.role;
        playback.stopWhenRequestReady = false;
        playback.loopIterationsCompleted = 0;
        if (provider.requests.activeFrameToken.isValid()) {
            result.providerFrameTransport[index].cancelToken = provider.requests.activeFrameToken;
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
        }
        if (input.role == ImageViewportPageRole::Primary) {
            beginRoleRequest(input.role, DisplayRequestOrigin::Playback,
                { -1, -1, ProviderRequestTargetKind::Playback }, { -1, -1 }, false);
        } else {
            auto& secondary = request.roles[1].activeRequest;
            secondary.target = { -1, -1, ProviderRequestTargetKind::Playback };
            secondary.resolvedFrame = { -1, -1 };
            secondary.providerFrameToken = {};
        }
        playback.providerStartPending = input.role == ImageViewportPageRole::Primary;
        playback.position = -1;
        playback.phase = ImageViewportPlaybackPhase::Waiting;
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
        result.changes.diagnostics = request.clearDiagnostics();
        stageBuiltIn();
        markRequest();
    }
    const bool preservePosition = playback.role == input.role && playback.position >= 0
        && !playback.stopWhenRequestReady && playback.phase != ImageViewportPlaybackPhase::Stopped;
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
        request.clearDiagnostics();
        request.targetSpreadTerminal.clear();
        dispatchProvider();
        markRequest();
    }
    const auto phase = request.status == ImageViewportRequestStatus::Loading
        ? ImageViewportPlaybackPhase::Waiting
        : ImageViewportPlaybackPhase::Playing;
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
    if (playback.phase != ImageViewportPlaybackPhase::Playing || input.elapsedMilliseconds <= 0) {
        return result;
    }
    result.projectSchedule = true;

    const ImageViewportPageRole role = playback.role;
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
        input.elapsedMilliseconds, currentFrame, playback.position,
        effectiveLooping(playback, authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    playback.position = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming && target.displayTarget.frame == currentFrame
        && request.status == ImageViewportRequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewportPageRole::Primary) {
        if (playback.stopWhenRequestReady || target.reachedEnd) {
            playback.stopWhenRequestReady = false;
            playback.phase = ImageViewportPlaybackPhase::Stopped;
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
    if (role == ImageViewportPageRole::Primary) {
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
            if (role == ImageViewportPageRole::Secondary) {
                wait.requiresSecondary = true;
                wait.secondary.requestQueued = true;
            } else {
                wait.primary.requestQueued = true;
            }
            request.status = ImageViewportRequestStatus::Loading;
            request.reason = projectWaitReason(wait);
            const bool retained = (display.status == ImageViewportDisplayStatus::Ready
                                      || display.status == ImageViewportDisplayStatus::Retained)
                && hasDisplayedPayload(display);
            display.status = retained ? ImageViewportDisplayStatus::Retained
                                      : ImageViewportDisplayStatus::Empty;
            display.clearPendingRenderPayload();
            if (provider.session.sessionActive) {
                effect.cancelToken = provider.requests.activeFrameToken;
            }
            provider.requests.activeFrameToken = {};
            provider.requests.activeFrameRefinement = false;
            active.providerFrameToken = {};
            provider.requests.queuedFrameRequest = true;
            provider.requests.queuedFrameGeneration = request.sequenceGeneration;
            provider.requests.queuedFrameRequestId = active.identity.id;
            provider.requests.queuedFrame = active.target.frame;
            provider.requests.queuedPosition = active.target.position;
            provider.requests.queuedResolvedFrame = active.resolvedFrame;
            provider.requests.queuedFrameFromPlayback = true;
            provider.requests.queuedFrameTargetKind = active.target.providerTargetKind;
            effect.deferredEngineEvent
                = ViewportProviderDeferredEngineEvent::FlushQueuedFrameRequest;
        } else {
            auto allocation = access.allocateProviderRequestToken(role);
            effect.closeSession = allocation.closeSession;
            effect.sessionClose = allocation.sessionClose;
            mergeChanges(result.changes, allocation.changes);
            if (allocation.exhausted) {
                acceptedDispatch = false;
            } else {
                provider.requests.activeFrameToken = allocation.token;
                provider.requests.activeFrameRefinement = false;
                active.providerFrameToken = allocation.token;
                effect.sendCommand = provider.session.sessionActive;
                effect.command.token = allocation.token;
                effect.command.frame = active.resolvedFrame.frame;
                effect.command.position = active.target.position;
                effect.command.targetKind = active.target.providerTargetKind;
                effect.command.demand = access.providerDemand(role, input.geometry);
                provider.requests.lastFrameDemand = effect.command.demand;
                provider.requests.hasLastFrameDemand = true;
                request.status = ImageViewportRequestStatus::Loading;
                request.reason = ImageViewportRequestReason::ProviderWaiting;
                display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                              : ImageViewportDisplayStatus::Empty;
                display.clearPendingRenderPayload();
            }
        }
        if (acceptedDispatch) {
            playback.stopWhenRequestReady = target.reachedEnd;
            playback.phase = ImageViewportPlaybackPhase::Waiting;
        }
    } else {
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(
            request.sequenceGeneration, request.roles[0].activeRequest);
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
                request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (request.roles[1].source.facts.present && !request.roles[1].source.facts.provider
            && request.roles[1].activeRequest.target.frame >= 0) {
            PreparedPayload secondary;
            secondary.commitPending = true;
            secondary.generation = request.sequenceGeneration;
            secondary.requestId = request.roles[0].activeRequest.identity.id;
            secondary.payloadId = ++display.nextPreparedPayloadId;
            request.roles[1].activeRequest.preparedPayloadId = secondary.payloadId;
            display.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
                request.roles[1].source, request.roles[1].activeRequest.target.frame, secondary)
                                                        .preparedPayload;
        }
        request.status = ImageViewportRequestStatus::Loading;
        request.reason = (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty())
            ? ImageViewportRequestReason::RenderWaiting
            : ImageViewportRequestReason::UploadPending;
        display.status = hasDisplayedPayload(display) ? ImageViewportDisplayStatus::Retained
                                                      : ImageViewportDisplayStatus::Empty;
        playback.stopWhenRequestReady = target.reachedEnd;
        playback.phase = ImageViewportPlaybackPhase::Waiting;
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
    using namespace ImageViewportInternal;
    enum class Eligibility {
        Pending,
        Ineligible,
        Eligible,
    };

    ViewportEngineAuthoredAutoplayReduction result;
    auto& playback = access.playback();
    if (playback.authoredAutoplayArbitration != AuthoredAutoplayArbitrationState::Pending) {
        return result;
    }

    const auto resolveWithoutDriver = [&] {
        playback.authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Resolved;
        result.resolved = true;
    };
    if (access.requestStatus() == ImageViewportRequestStatus::NoRequest
        || access.requestStatus() == ImageViewportRequestStatus::Unsupported
        || access.requestStatus() == ImageViewportRequestStatus::Error) {
        resolveWithoutDriver();
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

    const Eligibility primary = eligibility(ImageViewportPageRole::Primary);
    const Eligibility secondary = eligibility(ImageViewportPageRole::Secondary);
    if (primary == Eligibility::Pending || secondary == Eligibility::Pending) {
        return result;
    }

    playback.authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Resolved;
    result.resolved = true;
    const auto selected = primary == Eligibility::Eligible
        ? std::optional(ImageViewportPageRole::Primary)
        : secondary == Eligibility::Eligible ? std::optional(ImageViewportPageRole::Secondary)
                                             : std::nullopt;
    if (!selected) {
        return result;
    }

    const auto& source = access.source(*selected);
    const auto& intervals = source.facts.provider ? access.providerFacts(*selected).timingIntervals
                                                  : source.facts.timingIntervals;
    const auto previousPhase = playback.phase;
    playback.role = *selected;
    playback.position = intervals.frameStartPosition(access.activeRequest(*selected).target.frame);
    playback.stopWhenRequestReady = false;
    playback.providerStartPending = false;
    playback.loopIterationsCompleted = 0;
    playback.phase = access.requestStatus() == ImageViewportRequestStatus::Ready
        ? ImageViewportPlaybackPhase::Playing
        : ImageViewportPlaybackPhase::Waiting;
    result.armed = true;
    result.playbackPhaseChanged = previousPhase != playback.phase;
    return result;
}

ViewportPlaybackScheduleEffect projectViewportPlaybackSchedule(
    ViewportEnginePlaybackScheduleAccess access)
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    if (access.playback().phase != ImageViewportPlaybackPhase::Playing
        || access.request().status != ImageViewportRequestStatus::Ready) {
        return { Action::Stop, -1 };
    }

    const ImageViewportPageRole role = access.playback().role;
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
