#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprovidereventcompletionoperations_p.h"
#include "viewportengineproviderfailureoperations_p.h"
#include "viewportengineproviderframeoperations_p.h"
#include "viewportengineprovidermetadataoperations_p.h"
#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"

#include "viewportcontrollerprovidercontract_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <memory>

namespace {
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

ViewportEngineTransition ViewportEngine::handleProviderHostEvent(
    const ViewportEngineProviderHostEventRequest& input)
{
    const auto& event = input.event;
    ViewportEngineTransition result;
    switch (event.kind) {
    case ViewportProviderHostEvent::Kind::SessionOpened: {
        const auto opened = reduceProviderSessionOpened(event.role, input.viewport);
        appendProviderTransport(
            result.providerAfterPublication, opened.providerMetadataTransport, event.role);
        appendProviderTransport(
            result.providerAfterPublication, opened.providerFrameTransport, event.role);
        return result;
    }
    case ViewportProviderHostEvent::Kind::SessionOpenFailed: {
        const auto reduced = reduceProviderSessionOpenFailure(event.role, event.diagnostic);
        result.changes = reduced.changes;
        result.playbackSchedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::ProviderEvent: {
        const auto reduced = reduceProviderEvent(event.providerEvent, input.viewport);
        result.changes = reduced.changes;
        auto& batch = reduced.providerFrameTransportPhase
                == ViewportProviderEventTransportPhase::BeforeChanges
            ? result.providerBeforePublication
            : result.providerAfterPublication;
        appendProviderTransport(batch, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::DispatchFailed: {
        const auto reduced
            = reduceProviderDispatchFailure(event.role, { event.token, event.diagnostic });
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerAfterPublication, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest: {
        const auto reduced = reduceQueuedProviderFrameRequest(event.role, input.viewport);
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerAfterPublication, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed: {
        const auto reduced
            = reduceProviderQueueSchedulingFailure(event.role, event.diagnostic);
        result.changes = reduced.changes;
        result.providerSchedulerDiagnostic = reduced.diagnostic;
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    }
    return result;
}

ViewportEngineTransition ViewportEngine::handleDevicePixelRatioChanged(
    ViewportEngineViewportInput input)
{
    ViewportEngineTransition result;
    result.changes.displayRevision = true;
    result.changes.geometryState = true;
    result.changes.scheduleUpdate = true;
    const auto effects = restageProviderDemands(input);
    appendProviderTransport(result.providerAfterPublication, effects[0],
        ImageViewport::PageRole::Primary);
    appendProviderTransport(result.providerAfterPublication, effects[1],
        ImageViewport::PageRole::Secondary);
    return result;
}

ViewportProviderFrameTransportEffect ViewportEngine::closeProviderSession(
    ImageViewport::PageRole role)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderSessionCloseAccess access(provider.session, provider.requests);
    return closeViewportEngineProviderSession(std::move(access));
}

ViewportProviderSessionOpenResult ViewportEngine::reduceProviderSessionOpened(
    ImageViewport::PageRole role, ViewportEngineViewportInput input)
{
    const GeometryInput geometry = acceptedGeometry(input);
    ViewportEngineProviderSessionOpenedAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    return reduceViewportEngineProviderSessionOpened(
        { role, geometry }, std::move(access));
}

ViewportProviderFrameQueueFlushResult ViewportEngine::reduceQueuedProviderFrameRequest(
    ImageViewport::PageRole role, ViewportEngineViewportInput input)
{
    const GeometryInput geometry = acceptedGeometry(input);
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
    ViewportEngineViewportInput input)
{
    return restageProviderDemands(acceptedGeometry(input));
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
    const ViewportProviderEvent& event, ViewportEngineViewportInput input)
{
    const GeometryInput geometry = acceptedGeometry(input);
    auto& eventProvider = m_state->providerState.roles[roleIndex(event.role)].provider;
    ViewportEngineProviderSessionAdmissionAccess sessionAccess(
        m_state->requestState.request.sequenceGeneration, eventProvider.session);
    if (!acceptsViewportEngineProviderSessionEvent(
            { event.generation, event.sessionSerial }, std::move(sessionAccess))) {
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
    case ViewportProviderEvent::Kind::Progress: {
        auto& p = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderWaitingAccess access(
            m_state->requestState.request, p.facts, p.session, p.requests);
        result.changes = reduceViewportEngineProviderWaiting(
            { event.role, event.token, event.kind == ViewportProviderEvent::Kind::Progress,
                event.progress },
            std::move(access)).changes;
        break;
    }
    case ViewportProviderEvent::Kind::EndOfSequence: {
        ViewportEngineProviderEndOfSequenceAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        const auto eos = reduceViewportEngineProviderEndOfSequence(
            { event.role, event.token, geometry }, std::move(access));
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
