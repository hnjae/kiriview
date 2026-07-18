#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprovidereventcompletionoperations_p.h"
#include "viewportengineproviderfailureoperations_p.h"
#include "viewportengineproviderframeoperations_p.h"
#include "viewportengineprovidermetadataoperations_p.h"
#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportengineproviderterminaloperations_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

#include "imageviewporttoken_p.h"
#include "viewportprovidercontract_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <memory>

namespace {
ViewportEngineProviderTerminalEventInput terminalEvent(const ViewportProviderEvent& event,
    const ImageViewportInternal::PublicDiagnosticText& diagnostic)
{
    ViewportEngineProviderTerminalEventInput result;
    result.role = event.role;
    result.token = event.token;
    result.unsupportedCause = event.unsupportedCause;
    result.diagnostic = diagnostic;
    result.kind = event.kind == ImageSequenceProviderEventKind::Unsupported
        ? ViewportEngineProviderTerminalEventInput::Kind::Unsupported
        : event.kind == ImageSequenceProviderEventKind::Cancelled
        ? ViewportEngineProviderTerminalEventInput::Kind::Cancellation
        : ViewportEngineProviderTerminalEventInput::Kind::Failure;
    return result;
}

bool unsupportedCauseValid(ImageSequenceProviderUnsupportedCause cause)
{
    return cause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
        || cause == ImageSequenceProviderUnsupportedCause::PayloadRejection;
}

bool eventShapeCompatible(const ViewportProviderEvent& event)
{
    return event.kind != ImageSequenceProviderEventKind::Unsupported
        || unsupportedCauseValid(event.unsupportedCause);
}

bool eventKindCompatible(
    ImageSequenceProviderRequestKind requestKind, ImageSequenceProviderEventKind eventKind)
{
    switch (eventKind) {
    case ImageSequenceProviderEventKind::MetadataReady:
        return requestKind == ImageSequenceProviderRequestKind::Metadata;
    case ImageSequenceProviderEventKind::FrameReady:
        return requestKind == ImageSequenceProviderRequestKind::Frame
            || requestKind == ImageSequenceProviderRequestKind::Position
            || requestKind == ImageSequenceProviderRequestKind::Playback;
    case ImageSequenceProviderEventKind::EndOfSequence:
        return requestKind == ImageSequenceProviderRequestKind::Playback;
    case ImageSequenceProviderEventKind::Waiting:
    case ImageSequenceProviderEventKind::Progress:
    case ImageSequenceProviderEventKind::Failed:
    case ImageSequenceProviderEventKind::Unsupported:
    case ImageSequenceProviderEventKind::Cancelled:
        return requestKind == ImageSequenceProviderRequestKind::Metadata
            || requestKind == ImageSequenceProviderRequestKind::Frame
            || requestKind == ImageSequenceProviderRequestKind::Position
            || requestKind == ImageSequenceProviderRequestKind::Playback;
    }
    return false;
}

void commitProviderRequestMutation(
    ViewportEngineCanonicalState& state, ViewportEngineProviderRequestMutation mutation)
{
    state.requestState.request = std::move(mutation.request);
    state.playbackState.playback = std::move(mutation.playback);
    state.displayState.display = std::move(mutation.display);
    state.providerState.roles = std::move(mutation.roles);
    state.revisions.nextRevision = mutation.nextRevision;
}
}

ViewportEngineTransition ViewportEngine::handleProviderHostEvent(
    const ViewportEngineProviderHostEventRequest& input)
{
    const auto& event = input.event();
    const auto& diagnostic = input.diagnostic();
    ViewportEngineTransitionDraft result;
    switch (event.kind) {
    case ViewportProviderHostEvent::Kind::SessionOpened: {
        const auto opened = reduceProviderSessionOpened(event.role);
        appendProviderTransport(
            result.providerTransport, opened.providerMetadataTransport, event.role);
        appendProviderTransport(
            result.providerTransport, opened.providerFrameTransport, event.role);
        return finalizeTransition(std::move(result));
    }
    case ViewportProviderHostEvent::Kind::SessionOpenFailed: {
        const auto reduced = reduceProviderSessionOpenFailure(event.role, diagnostic);
        result.changes = reduced.changes;
        result.playbackSchedule = reduced.schedule;
        return finalizeTransition(std::move(result));
    }
    case ViewportProviderHostEvent::Kind::ProviderEvent: {
        const auto reduced = reduceProviderEvent(event.providerEvent, diagnostic);
        result.changes = reduced.changes;
        result.observations = reduced.observations;
        appendProviderTransport(
            result.providerTransport, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return finalizeTransition(std::move(result));
    }
    case ViewportProviderHostEvent::Kind::DispatchFailed: {
        const auto reduced = reduceProviderDispatchFailure(event.role, { event.token, diagnostic });
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerTransport, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return finalizeTransition(std::move(result));
    }
    case ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest: {
        const auto reduced = reduceQueuedProviderFrameRequest(event.role);
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerTransport, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return finalizeTransition(std::move(result));
    }
    case ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed: {
        const auto reduced = reduceProviderQueueSchedulingFailure(event.role, diagnostic);
        result.changes = reduced.changes;
        result.providerSchedulerDiagnostic = reduced.diagnostic;
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return finalizeTransition(std::move(result));
    }
    }
    return finalizeTransition(std::move(result));
}

bool ViewportEngine::acceptsProviderTransportCommand(
    const ViewportProviderTransportCommand& command) const
{
    const auto& provider = m_state->providerState.roles[roleIndex(command.role)].provider;
    switch (command.kind) {
    case ViewportProviderTransportCommand::Kind::OpenSession:
        return command.generation != 0
            && command.generation == m_state->requestState.request.sequenceGeneration
            && viewportEngineRoleCanRefineCurrentTerminal(
                m_state->requestState.request, command.role)
            && provider.session.sessionActive
            && command.sessionSerial == provider.session.sessionSerial;
    case ViewportProviderTransportCommand::Kind::CloseSession:
        return true;
    case ViewportProviderTransportCommand::Kind::ScheduleDeferredEvent:
        return provider.session.sessionActive;
    case ViewportProviderTransportCommand::Kind::SendRequest:
        break;
    }

    switch (command.request.kind()) {
    case ImageSequenceProviderRequestKind::Cancel:
    case ImageSequenceProviderRequestKind::Close:
        return true;
    case ImageSequenceProviderRequestKind::Metadata:
    case ImageSequenceProviderRequestKind::Frame:
    case ImageSequenceProviderRequestKind::Position:
    case ImageSequenceProviderRequestKind::Playback: {
        const auto* record = provider.requests.find(command.request.token());
        return provider.session.sessionActive && record && record->kind == command.request.kind();
    }
    }
    return false;
}

ViewportProviderFrameTransportEffect ViewportEngine::closeProviderSession(
    ImageViewportPageRole role)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderSessionCloseAccess access(provider.session, provider.requests);
    auto effect = closeViewportEngineProviderSession(access);
    auto mutation = access.takeMutation();
    provider.session = std::move(mutation.session);
    provider.requests = std::move(mutation.requests);
    return effect;
}

ViewportProviderTransportBatch ViewportEngine::shutdown()
{
    ViewportProviderTransportBatch effects;
    appendProviderTransport(effects, closeProviderSession(ImageViewportPageRole::Primary),
        ImageViewportPageRole::Primary);
    appendProviderTransport(effects, closeProviderSession(ImageViewportPageRole::Secondary),
        ImageViewportPageRole::Secondary);
    return effects;
}

ViewportProviderSessionOpenResult ViewportEngine::reduceProviderSessionOpened(
    ImageViewportPageRole role)
{
    const GeometryInput geometry = acceptedGeometry();
    ViewportEngineProviderSessionOpenedAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto result = reduceViewportEngineProviderSessionOpened({ role, geometry }, access);
    commitProviderRequestMutation(*m_state, access.takeMutation());
    return result;
}

ViewportProviderFrameQueueFlushResult ViewportEngine::reduceQueuedProviderFrameRequest(
    ImageViewportPageRole role)
{
    const GeometryInput geometry = acceptedGeometry();
    ViewportEngineProviderQueueFlushAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto result = reduceViewportEngineProviderQueueFlush({ role, geometry }, access);
    commitProviderRequestMutation(*m_state, access.takeMutation());
    if (result.changes.requestState) {
        result.schedule = currentPlaybackSchedule();
    }
    return result;
}

std::array<ViewportProviderFrameTransportEffect, 2> ViewportEngine::restageProviderDemands()
{
    return restageProviderDemands(acceptedGeometry());
}

std::array<ViewportProviderFrameTransportEffect, 2> ViewportEngine::restageProviderDemands(
    const GeometryInput& geometry)
{
    ViewportEngineProviderDemandRestageAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto effects = reduceViewportEngineProviderDemandRestage({ geometry }, access);
    commitProviderRequestMutation(*m_state, access.takeMutation());
    return effects;
}

ViewportProviderEventResult ViewportEngine::reduceProviderEvent(const ViewportProviderEvent& event,
    const ImageViewportInternal::PublicDiagnosticText& diagnostic)
{
    const GeometryInput geometry = event.kind == ImageSequenceProviderEventKind::MetadataReady
        ? rawAcceptedGeometry()
        : acceptedGeometry();
    auto& eventProvider = m_state->providerState.roles[roleIndex(event.role)].provider;
    ViewportEngineProviderSessionAdmissionAccess sessionAccess(
        m_state->requestState.request.sequenceGeneration, eventProvider.session);
    if (!acceptsViewportEngineProviderSessionEvent(
            { event.generation, event.sessionSerial }, std::move(sessionAccess))) {
        ViewportProviderEventResult stale;
        ImageViewportInternal::InternalObservation observation;
        observation.subsystem = ImageViewportInternal::InternalObservationSubsystem::Engine;
        observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
        observation.cause = ImageViewportInternal::InternalObservationCause::RetiredProviderSession;
        observation.identity.roleValid = true;
        observation.identity.role = event.role;
        observation.identity.generation = event.generation;
        observation.identity.sessionSerial = event.sessionSerial;
        observation.identity.providerToken
            = ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(event.token);
        stale.observations.append(observation);
        return stale;
    }
    const auto admission = eventProvider.requests.admit(event.token);
    if (admission.kind == ImageViewportInternal::ProviderRequestTokenAdmissionKind::Mismatch) {
        const auto reduced = reduceProviderProtocolViolation(event.role, event.token,
            ImageViewportInternal::InternalObservationCause::ProviderProtocolTokenMismatch,
            event.kind);
        ViewportProviderEventResult violation;
        violation.changes = reduced.changes;
        violation.providerFrameTransport = reduced.providerFrameTransport;
        violation.schedule = reduced.schedule;
        violation.observations = reduced.observations;
        return violation;
    }
    if (admission.kind == ImageViewportInternal::ProviderRequestTokenAdmissionKind::Retired) {
        ViewportProviderEventResult stale;
        ImageViewportInternal::InternalObservation observation;
        observation.subsystem = ImageViewportInternal::InternalObservationSubsystem::Engine;
        observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
        observation.cause = ImageViewportInternal::InternalObservationCause::ProviderTokenMismatch;
        observation.identity.roleValid = true;
        observation.identity.role = event.role;
        observation.identity.generation = event.generation;
        observation.identity.sessionSerial = event.sessionSerial;
        const auto& active
            = m_state->requestState.request.roles[roleIndex(event.role)].activeRequest;
        observation.identity.requestId = active.identity.id;
        observation.identity.providerToken
            = ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(event.token);
        observation.identity.demandRevision
            = ImageViewportInternal::DemandRevisionTokenPrivateAccess::value(active.demandRevision);
        observation.identity.providerLeaseId = event.frameLeaseId;
        stale.observations.append(observation);
        return stale;
    }
    const auto* providerRequest = admission.record;
    Q_ASSERT(providerRequest);
    ImageViewportInternal::InternalObservationCause violationCause
        = ImageViewportInternal::InternalObservationCause::None;
    if (providerRequest->role != event.role) {
        violationCause
            = ImageViewportInternal::InternalObservationCause::ProviderProtocolRoleMismatch;
    } else if (providerRequest->generation != event.generation) {
        violationCause
            = ImageViewportInternal::InternalObservationCause::ProviderProtocolGenerationMismatch;
    } else if (!eventKindCompatible(providerRequest->kind, event.kind)) {
        violationCause
            = ImageViewportInternal::InternalObservationCause::ProviderProtocolEventKindMismatch;
    } else if (!eventShapeCompatible(event)) {
        violationCause
            = ImageViewportInternal::InternalObservationCause::ProviderProtocolEventShapeMismatch;
    }
    if (violationCause != ImageViewportInternal::InternalObservationCause::None) {
        const auto reduced
            = reduceProviderProtocolViolation(event.role, event.token, violationCause, event.kind);
        ViewportProviderEventResult violation;
        violation.changes = reduced.changes;
        violation.providerFrameTransport = reduced.providerFrameTransport;
        violation.schedule = reduced.schedule;
        violation.observations = reduced.observations;
        return violation;
    }
    ViewportProviderEventResult result;
    switch (event.kind) {
    case ImageSequenceProviderEventKind::MetadataReady: {
        ViewportEngineProviderMetadataReadyAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->requestState.presentationTarget, m_state->revisions.nextRevision,
            m_state->revisions.targetPresentationRevision);
        const auto metadata = reduceViewportEngineProviderMetadataReady(
            { event.role, event.token, event.metadata, geometry }, access);
        auto mutation = access.takeMutation();
        m_state->requestState.request = std::move(mutation.request);
        m_state->playbackState.playback = std::move(mutation.playback);
        m_state->displayState.display = std::move(mutation.display);
        m_state->providerState.roles = std::move(mutation.roles);
        m_state->presentationState.presentation = std::move(mutation.presentation);
        m_state->requestState.presentationTarget = std::move(mutation.presentationTarget);
        m_state->revisions.nextRevision = mutation.nextRevision;
        m_state->revisions.targetPresentationRevision = mutation.targetPresentationRevision;
        result.changes = metadata.changes;
        result.observations = metadata.observations;
        result.providerFrameTransport = metadata.providerFrameTransport;
        break;
    }
    case ImageSequenceProviderEventKind::FrameReady: {
        auto& provider = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderFrameReadyAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display, provider,
            m_state->presentationState.presentation);
        const auto frame = reduceViewportEngineProviderFrameReady(
            { event.role, event.token, event.frameHandle ? event.frameHandle->frame() : nullptr,
                event.frameLeaseId, event.frameEnvelope, geometry },
            access);
        auto mutation = access.takeMutation();
        m_state->requestState.request = std::move(mutation.request);
        m_state->playbackState.playback = std::move(mutation.playback);
        m_state->displayState.display = std::move(mutation.display);
        provider = std::move(mutation.provider);
        result.changes = frame.changes;
        result.observations = frame.observations;
        break;
    }
    case ImageSequenceProviderEventKind::Waiting:
    case ImageSequenceProviderEventKind::Progress: {
        auto& p = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderWaitingAccess access(
            m_state->requestState.request, p.facts, p.session, p.requests);
        const auto waiting = reduceViewportEngineProviderWaiting(
            { event.role, event.token, event.kind == ImageSequenceProviderEventKind::Progress,
                event.progress },
            access);
        m_state->requestState.request = std::move(access.takeMutation().request);
        result.changes = waiting.changes;
        break;
    }
    case ImageSequenceProviderEventKind::EndOfSequence: {
        ViewportEngineProviderEndOfSequenceAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
            m_state->requestState.presentationTarget.generation);
        const auto eos = reduceViewportEngineProviderEndOfSequence(
            { event.role, event.token, geometry }, access);
        auto mutation = access.takeMutation();
        m_state->requestState.request = std::move(mutation.request);
        m_state->playbackState.playback = std::move(mutation.playback);
        m_state->displayState.display = std::move(mutation.display);
        m_state->providerState.roles = std::move(mutation.roles);
        m_state->revisions.nextRevision = mutation.nextRevision;
        result.changes = eos.changes;
        result.providerFrameTransport = eos.providerFrameTransport;
        result.observations = eos.observations;
        break;
    }
    case ImageSequenceProviderEventKind::Failed:
    case ImageSequenceProviderEventKind::Unsupported:
    case ImageSequenceProviderEventKind::Cancelled: {
        auto& provider = m_state->providerState.roles[roleIndex(event.role)].provider;
        ViewportEngineProviderTerminalEventAccess access(m_state->requestState.request,
            m_state->playbackState.playback, provider.facts, provider.session, provider.requests);
        const auto terminal
            = reduceViewportEngineProviderTerminalEvent(terminalEvent(event, diagnostic), access);
        auto mutation = access.takeMutation();
        m_state->requestState.request = std::move(mutation.request);
        m_state->playbackState.playback = std::move(mutation.playback);
        provider.session = std::move(mutation.session);
        provider.requests = std::move(mutation.requests);
        result.changes = terminal.changes;
        result.providerFrameTransport = terminal.providerFrameTransport;
        break;
    }
    }
    result.schedule = currentPlaybackSchedule();
    return result;
}

ViewportProviderSessionOpenFailureResult ViewportEngine::reduceProviderSessionOpenFailure(
    ImageViewportPageRole role, const ImageViewportInternal::PublicDiagnosticText& diagnostic)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderSessionOpenFailureAccess access(m_state->requestState.request,
        m_state->playbackState.playback, provider.session, provider.requests);
    const auto reduction
        = reduceViewportEngineProviderSessionOpenFailure({ role, diagnostic }, access);
    auto mutation = access.takeMutation();
    m_state->requestState.request = std::move(mutation.request);
    m_state->playbackState.playback = std::move(mutation.playback);
    provider.session = std::move(mutation.session);
    provider.requests = std::move(mutation.requests);
    ViewportProviderSessionOpenFailureResult result;
    result.changes = reduction.changes;
    if (result.changes.playbackPhase) {
        result.schedule = currentPlaybackSchedule();
    }
    return result;
}

ViewportProviderTerminalEventResult ViewportEngine::reduceProviderProtocolViolation(
    ImageViewportPageRole role, ImageSequenceProviderRequestToken token,
    ImageViewportInternal::InternalObservationCause cause, ImageSequenceProviderEventKind eventKind)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderProtocolViolationAccess access(m_state->requestState.request,
        m_state->playbackState.playback, provider.session, provider.requests);
    const auto reduction
        = reduceViewportEngineProviderProtocolViolation({ role, token, cause, eventKind }, access);
    auto mutation = access.takeMutation();
    m_state->requestState.request = std::move(mutation.request);
    m_state->playbackState.playback = std::move(mutation.playback);
    provider.session = std::move(mutation.session);
    provider.requests = std::move(mutation.requests);
    ViewportProviderTerminalEventResult result;
    result.changes = reduction.changes;
    result.providerFrameTransport = reduction.providerFrameTransport;
    result.schedule = currentPlaybackSchedule();
    result.observations = reduction.observations;
    return result;
}

ViewportProviderTerminalEventResult ViewportEngine::reduceProviderDispatchFailure(
    ImageViewportPageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderDispatchFailureAccess access(m_state->requestState.request,
        m_state->playbackState.playback, provider.session, provider.requests);
    const auto reduction = reduceViewportEngineProviderDispatchFailure(
        { role, event.token, event.diagnostic }, access);
    auto mutation = access.takeMutation();
    m_state->requestState.request = std::move(mutation.request);
    m_state->playbackState.playback = std::move(mutation.playback);
    provider.session = std::move(mutation.session);
    provider.requests = std::move(mutation.requests);
    ViewportProviderTerminalEventResult result;
    result.changes = reduction.changes;
    result.providerFrameTransport = reduction.providerFrameTransport;
    result.schedule = currentPlaybackSchedule();
    return result;
}

ViewportProviderSchedulerFailureResult ViewportEngine::reduceProviderQueueSchedulingFailure(
    ImageViewportPageRole role, const ImageViewportInternal::PublicDiagnosticText& diagnostic)
{
    auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    ViewportEngineProviderQueueFailureAccess access(
        m_state->requestState.request, m_state->playbackState.playback, provider.requests);
    const auto reduction = reduceViewportEngineProviderQueueFailure({ role, diagnostic }, access);
    auto mutation = access.takeMutation();
    m_state->requestState.request = std::move(mutation.request);
    m_state->playbackState.playback = std::move(mutation.playback);
    provider.requests = std::move(mutation.requests);
    ViewportProviderSchedulerFailureResult result;
    result.changes = reduction.changes;
    result.diagnostic = reduction.diagnostic;
    result.schedule = currentPlaybackSchedule();
    return result;
}
