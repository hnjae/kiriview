#include "viewportengineproviderfailureoperations_p.h"

#include "framepreparation_p.h"
#include "viewportengineprovidersessionoperations_p.h"

namespace {
using namespace ImageViewportInternal;

struct TerminalProjection
{
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
    QString diagnostic;
    QString fallbackDiagnostic;
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
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}

bool unsupportedCauseValid(ImageSequenceProviderUnsupportedCause cause)
{
    return cause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
        || cause == ImageSequenceProviderUnsupportedCause::PayloadRejection;
}

bool invalidUnsupportedCause(const ViewportEngineProviderTerminalEventInput& input)
{
    return input.kind == ViewportEngineProviderTerminalEventInput::Kind::Unsupported
        && input.unsupportedCauseExplicit && !unsupportedCauseValid(input.unsupportedCause);
}

TerminalProjection frameTerminal(const ViewportEngineProviderTerminalEventInput& input)
{
    if (input.kind == ViewportEngineProviderTerminalEventInput::Kind::Unsupported) {
        if (invalidUnsupportedCause(input)) {
            return { ImageViewportRequestStatus::Error,
                ImageViewportRequestReason::PayloadRejection, {},
                QStringLiteral("provider protocol violation") };
        }
        return { ImageViewportRequestStatus::Unsupported,
            input.unsupportedCause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
                ? ImageViewportRequestReason::UnsupportedRequest
                : ImageViewportRequestReason::PayloadRejection,
            input.diagnostic, QStringLiteral("provider unsupported") };
    }
    return { ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        input.diagnostic,
        input.kind == ViewportEngineProviderTerminalEventInput::Kind::Cancellation
            ? QStringLiteral("provider cancelled request")
            : QStringLiteral("provider failure") };
}

TerminalProjection metadataTerminal(const ViewportEngineProviderTerminalEventInput& input)
{
    if (input.kind == ViewportEngineProviderTerminalEventInput::Kind::Unsupported) {
        if (invalidUnsupportedCause(input)) {
            return { ImageViewportRequestStatus::Error,
                ImageViewportRequestReason::PayloadRejection, {},
                QStringLiteral("provider protocol violation") };
        }
        return { ImageViewportRequestStatus::Unsupported,
            input.unsupportedCauseExplicit
                    && input.unsupportedCause
                        == ImageSequenceProviderUnsupportedCause::PayloadRejection
                ? ImageViewportRequestReason::PayloadRejection
                : ImageViewportRequestReason::UnsupportedRequest,
            input.diagnostic, QStringLiteral("provider unsupported") };
    }
    return { ImageViewportRequestStatus::Error, ImageViewportRequestReason::ProviderFailure,
        input.diagnostic,
        input.kind == ViewportEngineProviderTerminalEventInput::Kind::Cancellation
            ? QStringLiteral("provider cancelled request")
            : QStringLiteral("provider failure") };
}

struct TerminalContext
{
    RequestState& request;
    PlaybackState& playback;
    const ProviderFactsState& facts;
    ProviderSessionState& session;
    ProviderRequestLedger& requests;
};

template <typename RecordTerminal, typename CloseSession>
ViewportEngineProviderTerminalEventReduction reduceTerminal(TerminalContext context,
    const ViewportEngineProviderTerminalEventInput& input, bool requireActiveSession,
    RecordTerminal recordTerminal, CloseSession closeSession)
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
        if (refinement && !invalidUnsupportedCause(input))
            return result;
        result.changes = recordTerminal({ input.role, terminal.status, terminal.reason,
            FailureScope::DisplayRequest,
            FramePreparation::boundedDiagnostic(terminal.diagnostic, terminal.fallbackDiagnostic),
            result.changes });
        updatePlaybackPhase(context.playback, ImageViewportPlaybackPhase::Stopped, result.changes);
        if (invalidUnsupportedCause(input)) {
            result.providerFrameTransport = closeSession();
        }
        return result;
    }

    if (context.facts.metadataReady || !providerRequest || !providerRequest->isMetadata()) {
        return result;
    }
    const auto terminal = metadataTerminal(input);
    context.requests.retire(input.token);
    context.playback.providerStartPending = false;
    result.changes
        = recordTerminal({ input.role, terminal.status, terminal.reason, FailureScope::Generation,
            FramePreparation::boundedDiagnostic(terminal.diagnostic, terminal.fallbackDiagnostic),
            result.changes });
    updatePlaybackPhase(context.playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = closeSession();
    return result;
}
}

#define DEFINE_RECORD_TERMINAL(Type)                                                               \
    ImageViewportInternal::ViewportChangeSet Type::recordTerminal(                                 \
        ViewportEngineProviderTerminalProjectionInput input)                                       \
    {                                                                                              \
        ViewportEngineProviderTerminalProjectionAccess access(m_request);                          \
        return reduceViewportEngineProviderTerminalProjection(                                     \
            std::move(input), std::move(access));                                                  \
    }

DEFINE_RECORD_TERMINAL(ViewportEngineProviderTerminalEventAccess)
DEFINE_RECORD_TERMINAL(ViewportEngineProviderDispatchFailureAccess)
DEFINE_RECORD_TERMINAL(ViewportEngineProviderSessionOpenFailureAccess)
DEFINE_RECORD_TERMINAL(ViewportEngineProviderQueueFailureAccess)
#undef DEFINE_RECORD_TERMINAL

#define DEFINE_CLOSE_SESSION(Type)                                                                 \
    ViewportProviderFrameTransportEffect Type::closeSession()                                      \
    {                                                                                              \
        ViewportEngineProviderSessionCloseAccess access(m_session, m_requests);                    \
        return closeViewportEngineProviderSession(std::move(access));                              \
    }

DEFINE_CLOSE_SESSION(ViewportEngineProviderTerminalEventAccess)
DEFINE_CLOSE_SESSION(ViewportEngineProviderDispatchFailureAccess)
#undef DEFINE_CLOSE_SESSION

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
    ViewportEngineProviderTerminalEventInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderTerminalEventAccess access)
{
    return reduceTerminal(
        { access.m_request, access.m_playback, access.m_facts, access.m_session,
            access.m_requests },
        input, true,
        [&access](ViewportEngineProviderTerminalProjectionInput terminal) {
            return access.recordTerminal(std::move(terminal));
        },
        [&access] { return access.closeSession(); });
}

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
    ViewportEngineProviderDispatchFailureInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderDispatchFailureAccess access)
{
    const bool sessionWasActive = access.m_session.sessionActive;
    const ViewportEngineProviderTerminalEventInput terminal { input.role, input.token,
        ViewportEngineProviderTerminalEventInput::Kind::Failure,
        ImageSequenceProviderUnsupportedCause::PayloadRejection,
        input.diagnostic.isEmpty() ? QStringLiteral("provider command delivery failed")
                                   : input.diagnostic,
        false };
    auto result = reduceTerminal(
        { access.m_request, access.m_playback, access.m_facts, access.m_session,
            access.m_requests },
        terminal, false,
        [&access](ViewportEngineProviderTerminalProjectionInput projection) {
            return access.recordTerminal(std::move(projection));
        },
        [&access, sessionWasActive] {
            if (sessionWasActive) {
                return access.closeSession();
            }
            access.m_requests.resetSession();
            return ViewportProviderFrameTransportEffect {};
        });
    if (sessionWasActive && result.changes.requestState
        && !result.providerFrameTransport.closeSession) {
        result.providerFrameTransport = access.closeSession();
    } else if (!sessionWasActive && result.changes.requestState) {
        access.m_requests.clearQueue();
    }
    return result;
}

ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput
        input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderSessionOpenFailureAccess access)
{
    ViewportEngineProviderSessionOpenFailureReduction result;
    access.m_session.sessionActive = false;
    access.m_requests.resetSession();
    result.changes = access.recordTerminal({ input.role, ImageViewportRequestStatus::Error,
        ImageViewportRequestReason::ProviderFailure, FailureScope::Generation, input.diagnostic,
        result.changes });
    updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    return result;
}

ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderQueueFailureAccess access)
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
    result.changes = access.recordTerminal({ input.role, ImageViewportRequestStatus::Error,
        ImageViewportRequestReason::ProviderFailure, FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(
            input.diagnostic, QStringLiteral("provider command delivery failed")),
        result.changes });
    if (playbackOwned) {
        updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    }
    return result;
}
