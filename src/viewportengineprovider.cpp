#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "viewportcontrollerprovidercontract_p.h"

#include <cmath>
#include <memory>

namespace {
using namespace ImageViewportInternal;

ProviderRoleState& providerFor(
    std::array<ViewportEngineRoleState, 2>& roles, ImageViewport::PageRole role)
{
    return roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U].provider;
}

const DisplayRequest& requestForRole(const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.roles[1].activeRequest
                                                      : request.roles[0].activeRequest;
}

DisplayRequest& requestForRole(RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.roles[1].activeRequest
                                                      : request.roles[0].activeRequest;
}

bool effectiveProviderLooping(
    const PlaybackState& playback, ImageSequenceAuthoredAnimationFacts authored)
{
    if (playback.looping) {
        return true;
    }
    switch (authored.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return playback.loopIterationsCompleted + 1 < authored.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

void updatePlaybackPhase(PlaybackState& playback, ImageViewport::PlaybackPhase phase,
    ViewportChangeSet& changes)
{
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}

ViewportEngineProviderTerminalEventInput terminalEvent(const ViewportProviderEvent& event)
{
    ViewportEngineProviderTerminalEventInput result;
    result.role = event.role;
    result.token = event.token;
    result.unsupportedCause = event.unsupportedCause;
    result.diagnostic = event.diagnostic;
    result.unsupportedCauseExplicit = event.unsupportedCauseExplicit;
    result.kind = event.kind == ViewportProviderEvent::Kind::Unsupported
        ? ViewportEngineProviderTerminalEventInput::Kind::Unsupported
        : event.kind == ViewportProviderEvent::Kind::Cancellation
        ? ViewportEngineProviderTerminalEventInput::Kind::Cancellation
        : ViewportEngineProviderTerminalEventInput::Kind::Failure;
    return result;
}
}

ViewportProviderFrameTransportEffect ViewportEngine::closeProviderSession(
    ImageViewport::PageRole role)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderSessionCloseAccess access(provider.session, provider.requests);
    return closeViewportEngineProviderSession(std::move(access));
}

ViewportProviderSessionOpenResult ViewportEngine::reduceProviderSessionOpened(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ViewportEngineProviderSessionOpenedAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    return reduceViewportEngineProviderSessionOpened(
        { role, geometry }, std::move(access));
}

ViewportProviderFrameQueueFlushResult ViewportEngine::reduceQueuedProviderFrameRequest(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ViewportEngineProviderQueueFlushAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto result = reduceViewportEngineProviderQueueFlush(
        { role, geometry }, std::move(access));
    if (result.changes.requestState) {
        result.schedule = currentPlaybackSchedule();
    }
    return result;
}

std::array<ViewportProviderFrameTransportEffect, 2> ViewportEngine::restageProviderDemands(
    const GeometryInput& geometry)
{
    ViewportEngineProviderDemandRestageAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    return reduceViewportEngineProviderDemandRestage({ geometry }, std::move(access));
}

ViewportProviderEventResult ViewportEngine::reduceProviderEvent(
    const ViewportProviderEvent& event, const GeometryInput& geometry)
{
    if (!acceptsProviderSessionEvent(event.role, event.sessionSerial, event.generation)) {
        std::unique_ptr<ImageSequenceProviderFrameHandle> stale(event.frameHandle);
        return {};
    }
    ViewportProviderEventResult result;
    switch (event.kind) {
    case ViewportProviderEvent::Kind::MetadataReady: {
        ViewportEngineProviderMetadataReadyAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        const auto metadata = reduceViewportEngineProviderMetadataReady(
            { event.role, event.token, event.metadata, geometry }, std::move(access));
        result.changes = metadata.changes;
        result.providerFrameTransport = metadata.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::BeforeChanges;
        break;
    }
    case ViewportProviderEvent::Kind::ImageFrameReady:
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady: {
        auto& provider = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderFrameReadyAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display, provider,
            m_state->presentationState.presentation);
        result.changes = reduceViewportEngineProviderFrameReady(
            { event.role, event.token, event.imageFrame,
                event.kind == ViewportProviderEvent::Kind::ImageFrameReady
                    ? ImageSequenceProviderFrameMetadata::stillFrame()
                    : event.frameMetadata,
                geometry },
            std::move(access))
                             .changes;
        break;
    }
    case ViewportProviderEvent::Kind::FrameHandleReady:
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady: {
        std::unique_ptr<ImageSequenceProviderFrameHandle> owned(event.frameHandle);
        auto& provider = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderFrameReadyAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display, provider,
            m_state->presentationState.presentation);
        result.changes = reduceViewportEngineProviderFrameReady(
            { event.role, event.token, owned ? owned->frame() : nullptr,
                event.kind == ViewportProviderEvent::Kind::FrameHandleReady
                    ? ImageSequenceProviderFrameMetadata::stillFrame()
                    : event.frameMetadata,
                geometry },
            std::move(access))
                             .changes;
        break;
    }
    case ViewportProviderEvent::Kind::Waiting:
    case ViewportProviderEvent::Kind::Progress:
        result.changes = reduceProviderWaitingEvent(event.role,
            { event.token, event.kind == ViewportProviderEvent::Kind::Progress, event.progress });
        break;
    case ViewportProviderEvent::Kind::EndOfSequence: {
        const auto eos = reduceProviderEndOfSequence(event.role, { event.token }, geometry);
        result.changes = eos.changes;
        result.providerFrameTransport = eos.providerFrameTransport;
        result.providerFrameTransportPhase = eos.providerFrameTransport.closeSession
            ? ViewportProviderEventTransportPhase::AfterChanges
            : ViewportProviderEventTransportPhase::BeforeChanges;
        break;
    }
    case ViewportProviderEvent::Kind::Failure:
    case ViewportProviderEvent::Kind::Unsupported:
    case ViewportProviderEvent::Kind::Cancellation: {
        auto& provider = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderTerminalEventAccess access(m_state->requestState.request,
            m_state->playbackState.playback, provider.facts, provider.session, provider.requests);
        const auto terminal
            = reduceViewportEngineProviderTerminalEvent(terminalEvent(event), std::move(access));
        result.changes = terminal.changes;
        result.providerFrameTransport = terminal.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::AfterChanges;
        break;
    }
    }
    result.schedule = currentPlaybackSchedule();
    return result;
}

ViewportProviderSessionOpenFailureResult ViewportEngine::reduceProviderSessionOpenFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderSessionOpenFailureAccess access(m_state->requestState.request,
        m_state->playbackState.playback, provider.session, provider.requests);
    const auto reduction = reduceViewportEngineProviderSessionOpenFailure(
        { role, diagnostic }, std::move(access));
    ViewportProviderSessionOpenFailureResult result;
    result.changes = reduction.changes;
    if (result.changes.playbackPhase) {
        result.schedule = currentPlaybackSchedule();
    }
    return result;
}

ViewportProviderTerminalEventResult ViewportEngine::reduceProviderDispatchFailure(
    ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderDispatchFailureAccess access(m_state->requestState.request,
        m_state->playbackState.playback, provider.facts, provider.session, provider.requests);
    const auto reduction = reduceViewportEngineProviderDispatchFailure(
        { role, event.token, event.diagnostic }, std::move(access));
    ViewportProviderTerminalEventResult result;
    result.changes = reduction.changes;
    result.providerFrameTransport = reduction.providerFrameTransport;
    result.schedule = currentPlaybackSchedule();
    return result;
}

ViewportProviderSchedulerFailureResult ViewportEngine::reduceProviderQueueSchedulingFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderQueueFailureAccess access(
        m_state->requestState.request, m_state->playbackState.playback, provider.requests);
    const auto reduction
        = reduceViewportEngineProviderQueueFailure({ role, diagnostic }, std::move(access));
    ViewportProviderSchedulerFailureResult result;
    result.changes = reduction.changes;
    result.diagnostic = reduction.diagnostic;
    result.schedule = currentPlaybackSchedule();
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::reduceProviderWaitingEvent(
    ImageViewport::PageRole role, const ViewportProviderWaitingEvent& event)
{
    ViewportChangeSet changes;
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return changes;
    }
    auto& provider = providerFor(providerAccess().roles(), role);
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].provider;
    if (!providerPresent || !provider.session.sessionActive
        || (event.progress
            && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
                || event.progressValue > 1.0))) {
        return changes;
    }
    const bool metadataToken = !provider.facts.metadataReady
        && provider.requests.activeMetadataToken.isValid()
        && event.token == provider.requests.activeMetadataToken;
    const DisplayRequest& request = requestForRole(providerAccess().request(), role);
    const bool frameToken = provider.requests.activeFrameToken.isValid()
        && event.token == provider.requests.activeFrameToken
        && event.token == request.providerFrameToken;
    if ((!metadataToken && !frameToken)
        || providerAccess().request().status != ImageViewport::RequestStatus::Loading
        || providerAccess().request().reason == ImageViewport::RequestReason::ProviderWaiting) {
        return changes;
    }
    providerAccess().request().reason = ImageViewport::RequestReason::ProviderWaiting;
    changes.requestState = true;
    changes.requestRevision = true;
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportEngine::reduceProviderEndOfSequenceProtocolViolation(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation input)
{
    ViewportProviderEndOfSequenceResult result;
    clearQueuedProviderFrameRequest(role);
    auto& provider = providerFor(providerAccess().roles(), role);
    if (input.activeMetadataToken) {
        provider.requests.activeMetadataToken = {};
    }
    if (input.activeFrameToken) {
        provider.requests.activeFrameToken = {};
    }
    providerAccess().playback().providerStartPending = false;
    providerAccess().playback().stopWhenRequestReady = false;
    result.changes = reduceViewportEngineProviderTerminalProjection(
        { role, ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::PayloadRejection,
            input.activeMetadataToken ? FailureScope::Generation : FailureScope::DisplayRequest,
            QStringLiteral("provider protocol violation"), result.changes },
        ViewportEngineProviderTerminalProjectionAccess(m_state->requestState.request));
    updatePlaybackPhase(
        providerAccess().playback(), ImageViewport::PlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = closeProviderSession(role);
    return result;
}

ViewportProviderEndOfSequenceResult ViewportEngine::reduceProviderEndOfSequence(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event,
    const GeometryInput& geometry)
{
    ViewportProviderEndOfSequenceResult result;
    auto& provider = providerFor(providerAccess().roles(), role);
    const bool present = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].sequence
            && providerAccess().request().roles[1].provider;
    if (!present || !provider.session.sessionActive) {
        return result;
    }
    const bool metadataToken = !provider.facts.metadataReady
        && provider.requests.activeMetadataToken.isValid()
        && event.token == provider.requests.activeMetadataToken;
    const auto& request = requestForRole(providerAccess().request(), role);
    const bool frameToken = provider.requests.activeFrameToken.isValid()
        && event.token == provider.requests.activeFrameToken
        && event.token == request.providerFrameToken;
    if (!metadataToken && !frameToken) {
        return result;
    }
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return result;
    }
    if (metadataToken || !provider.facts.metadataReady || !provider.facts.timedMetadata
        || request.target.providerTargetKind != ProviderRequestTargetKind::Playback) {
        return reduceProviderEndOfSequenceProtocolViolation(role, { metadataToken, frameToken });
    }

    provider.requests.activeFrameToken = {};
    const bool diagnosticsChanged = providerAccess().request().clearDiagnostics();
    const bool loop = effectiveProviderLooping(
        providerAccess().playback(), provider.facts.authoredAnimationFacts);
    const int selectedFrame = loop ? 0 : provider.facts.timingIntervals.frameCount() - 1;
    const int selectedPosition
        = loop ? 0 : provider.facts.timingIntervals.frameStartPosition(selectedFrame);
    providerAccess().playback().position
        = loop ? 0 : provider.facts.timingIntervals.totalDuration();
    providerAccess().playback().stopWhenRequestReady = !loop;
    const DisplayRequestTarget target { selectedFrame, selectedPosition,
        ProviderRequestTargetKind::Playback };

    if (role == ImageViewport::PageRole::Secondary) {
        const auto primary = providerAccess().request().roles[0].activeRequest;
        providerAccess().request().beginDisplayRequest(
            DisplayRequestOrigin::Playback, primary.target, primary.resolvedFrame, false);
        providerAccess().request().roles[1].activeRequest.identity
            = providerAccess().request().roles[0].activeRequest.identity;
        providerAccess().request().roles[1].activeRequest.target = target;
        providerAccess().request().roles[1].activeRequest.resolvedFrame
            = { selectedFrame, selectedPosition };
        providerAccess().request().roles[1].activeRequest.providerFrameToken = {};
        providerAccess().request().roles[1].activeRequest.preparedPayloadId
            = providerAccess().request().roles[0].activeRequest.preparedPayloadId;
    } else {
        providerAccess().request().roles[0].activeRequest.target = target;
        providerAccess().request().roles[0].activeRequest.resolvedFrame
            = { selectedFrame, selectedPosition };
    }

    const bool sameDisplayedFinal = role == ImageViewport::PageRole::Primary && !loop
        && providerAccess().display().hasReadyDisplay(
            providerAccess().request().roles[0].source.facts.present)
        && providerAccess().display().roles[0].displayedRequest.generation
            == providerAccess().request().sequenceGeneration
        && providerAccess().display().roles[0].displayedRequest.request.resolvedFrame.frame
            == selectedFrame
        && providerAccess().display().roles[0].displayedRequest.request.resolvedFrame.position
            == selectedPosition;
    if (sameDisplayedFinal) {
        providerAccess().request().status = ImageViewport::RequestStatus::Ready;
        providerAccess().request().reason = ImageViewport::RequestReason::Ready;
        providerAccess().display().status = ImageViewport::DisplayStatus::Ready;
        updatePlaybackPhase(
            providerAccess().playback(), ImageViewport::PlaybackPhase::Stopped, result.changes);
        providerAccess().playback().stopWhenRequestReady = false;
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.diagnostics = diagnosticsChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    providerAccess().request().targetSpreadTerminal.clear();
    providerAccess().display().clearPendingRenderPayload();
    providerAccess().display().clearRenderFailureRetainedDisplay();
    ViewportEngineProviderFrameRequestAccess frameRequestAccess(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    const auto start = startViewportEngineProviderFrameRequest(
        { role, target, geometry }, std::move(frameRequestAccess));
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    if (loop && !providerAccess().playback().looping) {
        ++providerAccess().playback().loopIterationsCompleted;
    }
    updatePlaybackPhase(
        providerAccess().playback(), ImageViewport::PlaybackPhase::Waiting, result.changes);
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.diagnostics = diagnosticsChanged || !start.accepted;
    result.changes.scheduleUpdate = true;
    return result;
}
