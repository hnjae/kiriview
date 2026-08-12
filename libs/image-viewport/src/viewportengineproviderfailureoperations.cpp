// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineproviderfailureoperations_p.h"

#include "imageviewporttoken_p.h"
#include "viewportengineprovidersessionoperations_p.h"

namespace {
using namespace ImageViewportInternal;

struct TerminalProjection
{
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
    PublicDiagnosticText diagnostic;
};

const DisplayRequest& requestForRole(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
}

DisplayRequest& requestForRole(RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
}

bool providerPresent(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? request.roles[0].source.facts.provider
                                                  : request.roles[1].provider;
}

void updatePlaybackPhase(
    PlaybackState& playback, ImageViewportPlaybackPhase phase, ViewportChangeSet& changes)
{
    for (auto& rolePlayback : playback.roles) {
        if (rolePlayback.phase == phase) {
            continue;
        }
        rolePlayback.phase = phase;
        changes.playbackPhase = true;
    }
}

PublicDiagnosticText providerFailureDiagnostic(
    ImageSequenceProviderFailureCause cause, bool metadata)
{
    const QString operation = metadata ? QStringLiteral("metadata") : QStringLiteral("frame");
    switch (cause) {
    case ImageSequenceProviderFailureCause::SourceAccess:
        return PublicDiagnosticText::fromTrusted(
            QStringLiteral("provider source access failed for %1").arg(operation));
    case ImageSequenceProviderFailureCause::Decode:
        return PublicDiagnosticText::fromTrusted(
            QStringLiteral("provider decode failed for %1").arg(operation));
    case ImageSequenceProviderFailureCause::ResourceExhausted:
        return PublicDiagnosticText::fromTrusted(
            QStringLiteral("provider resources exhausted for %1").arg(operation));
    case ImageSequenceProviderFailureCause::ProviderInternal:
        return PublicDiagnosticText::fromTrusted(
            QStringLiteral("provider internal failure for %1").arg(operation));
    case ImageSequenceProviderFailureCause::Unavailable:
        return PublicDiagnosticText::fromTrusted(QStringLiteral("provider failure"));
    }
    return PublicDiagnosticText::fromTrusted(QStringLiteral("provider failure"));
}

TerminalProjection frameTerminal(const ViewportEngineProviderTerminalEventInput& input)
{
    if (input.kind == ViewportEngineProviderTerminalEventInput::Kind::Unsupported) {
        return { ImageViewportRequestStatus::Unsupported,
            input.unsupportedCause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
                ? ImageViewportRequestReason::UnsupportedRequest
                : ImageViewportRequestReason::PayloadRejection,
            PublicDiagnosticText::fromTrusted(QStringLiteral("provider unsupported")) };
    }
    return { ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        input.kind == ViewportEngineProviderTerminalEventInput::Kind::Cancellation
            ? PublicDiagnosticText::fromTrusted(QStringLiteral("provider cancelled request"))
            : providerFailureDiagnostic(input.providerCause, false) };
}

TerminalProjection metadataTerminal(const ViewportEngineProviderTerminalEventInput& input)
{
    if (input.kind == ViewportEngineProviderTerminalEventInput::Kind::Unsupported) {
        return { ImageViewportRequestStatus::Unsupported,
            input.unsupportedCause == ImageSequenceProviderUnsupportedCause::PayloadRejection
                ? ImageViewportRequestReason::PayloadRejection
                : ImageViewportRequestReason::UnsupportedRequest,
            PublicDiagnosticText::fromTrusted(QStringLiteral("provider unsupported")) };
    }
    return { ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        input.kind == ViewportEngineProviderTerminalEventInput::Kind::Cancellation
            ? PublicDiagnosticText::fromTrusted(QStringLiteral("provider cancelled request"))
            : providerFailureDiagnostic(input.providerCause, true) };
}

struct TerminalContext
{
    RequestState& request;
    PlaybackState& playback;
    const ProviderFactsState& facts;
    ProviderSessionState& session;
    ProviderRequestLedger& requests;
};

template <typename RecordDisplayRequestTerminal, typename RecordGenerationTerminal,
    typename CloseSession>
ViewportEngineProviderTerminalEventReduction reduceTerminal(TerminalContext context,
    const ViewportEngineProviderTerminalEventInput& input, bool requireActiveSession,
    RecordDisplayRequestTerminal recordDisplayRequestTerminal,
    RecordGenerationTerminal recordGenerationTerminal, CloseSession closeSession)
{
    ViewportEngineProviderTerminalEventReduction result;
    if (!providerPresent(context.request, input.role)
        || (requireActiveSession && !context.session.sessionActive)) {
        return result;
    }

    const auto& request = requestForRole(context.request, input.role);
    const auto* providerRequest = context.requests.find(input.token);
    const bool frameToken = providerRequest && providerRequest->isFrameWork()
        && providerRequest->generation == context.request.sequenceGeneration
        && providerRequest->requestId == request.identity.id;
    if (frameToken) {
        const bool refinement = providerRequest->isRefinement();
        const auto terminal = frameTerminal(input);
        context.requests.clearQueue();
        context.requests.retire(input.token);
        if (refinement)
            return result;
        result.changes = recordDisplayRequestTerminal({ input.role, terminal.status,
            terminal.reason, terminal.diagnostic, result.changes, input.providerFailureAvailable,
            input.providerCause, input.providerReference, input.providerFailureLeaseId });
        updatePlaybackPhase(context.playback, ImageViewportPlaybackPhase::Stopped, result.changes);
        return result;
    }

    if (context.facts.metadataReady || !providerRequest || !providerRequest->isMetadata()) {
        return result;
    }
    const auto terminal = metadataTerminal(input);
    context.requests.retire(input.token);
    context.playback.forRole(input.role).providerStartPending = false;
    result.changes = recordGenerationTerminal({ input.role, terminal.status, terminal.reason,
        terminal.diagnostic, result.changes, input.providerFailureAvailable, input.providerCause,
        input.providerReference, input.providerFailureLeaseId });
    updatePlaybackPhase(context.playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = closeSession();
    return result;
}
}

#define DEFINE_RECORD_DISPLAY_REQUEST_TERMINAL(Type)                                               \
    ImageViewportInternal::ViewportChangeSet Type::recordDisplayRequestTerminal(                   \
        ViewportEngineProviderTerminalProjectionInput input)                                       \
    {                                                                                              \
        ViewportEngineProviderTerminalProjectionAccess access(m_request);                          \
        auto changes = reduceViewportEngineProviderDisplayRequestTerminalProjection(               \
            std::move(input), access);                                                             \
        m_request = std::move(access.takeMutation().request);                                      \
        return changes;                                                                            \
    }

#define DEFINE_RECORD_GENERATION_TERMINAL(Type)                                                    \
    ImageViewportInternal::ViewportChangeSet Type::recordGenerationTerminal(                       \
        ViewportEngineProviderTerminalProjectionInput input)                                       \
    {                                                                                              \
        ViewportEngineProviderTerminalProjectionAccess access(m_request);                          \
        auto changes                                                                               \
            = reduceViewportEngineProviderGenerationTerminalProjection(std::move(input), access);  \
        m_request = std::move(access.takeMutation().request);                                      \
        return changes;                                                                            \
    }

DEFINE_RECORD_DISPLAY_REQUEST_TERMINAL(ViewportEngineProviderTerminalEventAccess)
DEFINE_RECORD_DISPLAY_REQUEST_TERMINAL(ViewportEngineProviderQueueFailureAccess)
DEFINE_RECORD_GENERATION_TERMINAL(ViewportEngineProviderTerminalEventAccess)
DEFINE_RECORD_GENERATION_TERMINAL(ViewportEngineProviderProtocolViolationAccess)
DEFINE_RECORD_GENERATION_TERMINAL(ViewportEngineProviderDispatchFailureAccess)
DEFINE_RECORD_GENERATION_TERMINAL(ViewportEngineProviderSessionOpenFailureAccess)
#undef DEFINE_RECORD_DISPLAY_REQUEST_TERMINAL
#undef DEFINE_RECORD_GENERATION_TERMINAL

#define DEFINE_CLOSE_SESSION(Type)                                                                 \
    ViewportProviderFrameTransportEffect Type::closeSession()                                      \
    {                                                                                              \
        ViewportEngineProviderSessionCloseAccess access(m_session, m_requests);                    \
        auto effect = closeViewportEngineProviderSession(access);                                  \
        auto mutation = access.takeMutation();                                                     \
        m_session = mutation.session;                                                              \
        m_requests = std::move(mutation.requests);                                                 \
        return effect;                                                                             \
    }

DEFINE_CLOSE_SESSION(ViewportEngineProviderTerminalEventAccess)
DEFINE_CLOSE_SESSION(ViewportEngineProviderProtocolViolationAccess)
DEFINE_CLOSE_SESSION(ViewportEngineProviderDispatchFailureAccess)
#undef DEFINE_CLOSE_SESSION

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
    ViewportEngineProviderTerminalEventInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderTerminalEventAccess& access)
{
    return reduceTerminal(
        { access.m_request, access.m_playback, access.m_facts, access.m_session,
            access.m_requests },
        input, true,
        [&access](ViewportEngineProviderTerminalProjectionInput terminal) {
            return access.recordDisplayRequestTerminal(std::move(terminal));
        },
        [&access](ViewportEngineProviderTerminalProjectionInput terminal) {
            return access.recordGenerationTerminal(std::move(terminal));
        },
        [&access] { return access.closeSession(); });
}

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderProtocolViolation(
    ViewportEngineProviderProtocolViolationInput
        input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderProtocolViolationAccess& access)
{
    ViewportEngineProviderTerminalEventReduction result;
    if (!providerPresent(access.m_request, input.role) || !access.m_session.sessionActive
        || access.m_requests.active.isEmpty()) {
        return result;
    }

    const auto& active = requestForRole(access.m_request, input.role);
    InternalObservation observation;
    observation.subsystem = InternalObservationSubsystem::Engine;
    observation.category = InternalObservationCategory::AdmissionFailure;
    observation.cause = input.cause;
    observation.identity.roleValid = true;
    observation.identity.role = input.role;
    observation.identity.generation = access.m_request.sequenceGeneration;
    observation.identity.sessionSerial = access.m_session.sessionSerial;
    observation.identity.requestId = active.identity.id;
    observation.identity.providerToken = ProviderRequestTokenPrivateAccess::value(input.token);
    observation.identity.demandRevision
        = DemandRevisionTokenPrivateAccess::value(active.demandRevision);
    observation.detail = int(input.eventKind);
    result.observations.append(observation);

    auto& rolePlayback = access.m_playback.forRole(input.role);
    rolePlayback.providerStartPending = false;
    rolePlayback.stopWhenRequestReady = false;
    if (access.m_requests.find(input.token)) {
        access.m_requests.retire(input.token);
    }
    result.changes = access.recordGenerationTerminal({ input.role,
        ImageViewportRequestStatus::Error, ImageViewportRequestReason::PayloadRejection,
        PublicDiagnosticText::fromTrusted(QStringLiteral("provider protocol violation")),
        result.changes });
    updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = access.closeSession();
    return result;
}

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
    ViewportEngineProviderDispatchFailureInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderDispatchFailureAccess& access)
{
    ViewportEngineProviderTerminalEventReduction result;
    const auto* providerRequest = access.m_requests.find(input.token);
    if (!providerPresent(access.m_request, input.role) || !providerRequest
        || providerRequest->generation != access.m_request.sequenceGeneration) {
        return result;
    }

    const bool sessionWasActive = access.m_session.sessionActive;
    auto& rolePlayback = access.m_playback.forRole(input.role);
    rolePlayback.providerStartPending = false;
    rolePlayback.stopWhenRequestReady = false;
    access.m_requests.retire(input.token);
    access.m_requests.clearQueue();
    result.changes = access.recordGenerationTerminal({ input.role,
        ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        PublicDiagnosticText::fromTrusted(QStringLiteral("provider command delivery failed")),
        result.changes });
    updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    if (sessionWasActive) {
        result.providerFrameTransport = access.closeSession();
    } else {
        access.m_requests.resetSession();
    }
    return result;
}

ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput
        input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderSessionOpenFailureAccess& access)
{
    ViewportEngineProviderSessionOpenFailureReduction result;
    access.m_session.sessionActive = false;
    access.m_session.sessionOpened = false;
    access.m_requests.resetSession();
    result.changes = access.recordGenerationTerminal({ input.role,
        ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        PublicDiagnosticText::fromTrusted(QStringLiteral("provider session creation failed")),
        result.changes, input.providerFailureAvailable, input.providerCause,
        input.providerReference, input.providerFailureLeaseId });
    updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    return result;
}

ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderQueueFailureAccess& access)
{
    ViewportEngineProviderQueueFailureReduction result;
    const auto& request = requestForRole(access.m_request, input.role);
    const auto queuedRequest = access.m_requests.queuedFrame;
    const bool queued = queuedRequest.has_value();
    result.diagnostic = { queued, input.role, queued ? queuedRequest->generation : 0,
        request.identity.id, queued ? queuedRequest->requestId : 0,
        queued ? queuedRequest->target.providerTargetKind : ProviderRequestTargetKind::Unknown,
        ProviderSchedulerOperation::FlushQueuedFrameRequest };
    if (!queued || !providerPresent(access.m_request, input.role)) {
        access.m_requests.clearQueue();
        return result;
    }

    const bool playbackOwned = queuedRequest->fromPlayback;
    access.m_requests.clearQueue();
    result.changes = access.recordDisplayRequestTerminal({ input.role,
        ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        PublicDiagnosticText::fromTrusted(
            QStringLiteral("provider queued request scheduling failed")),
        result.changes });
    if (playbackOwned) {
        updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    }
    return result;
}
