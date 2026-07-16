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

void clearQueue(ProviderRequestState& requests)
{
    requests.queuedFrameRequest = false;
    requests.queuedFrameGeneration = 0;
    requests.queuedFrameRequestId = 0;
    requests.queuedFrame = -1;
    requests.queuedPosition = -1;
    requests.queuedResolvedFrame = {};
    requests.queuedFrameFromPlayback = false;
    requests.queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
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
    ProviderRequestState& requests;
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
    const bool frameToken = input.token.isValid()
        && input.token == context.requests.activeFrameToken
        && input.token == request.providerFrameToken;
    if (frameToken) {
        const bool refinement = context.requests.activeFrameRefinement;
        const auto terminal = frameTerminal(input);
        clearQueue(context.requests);
        context.requests.activeFrameToken = {};
        context.requests.activeFrameRefinement = false;
        requestForRole(context.request, input.role).providerFrameToken = {};
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

    if (context.facts.metadataReady || !context.requests.activeMetadataToken.isValid()
        || input.token != context.requests.activeMetadataToken) {
        return result;
    }
    const auto terminal = metadataTerminal(input);
    context.requests.activeMetadataToken = {};
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
    ViewportEngineProviderTerminalEventInput input,
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
    ViewportEngineProviderDispatchFailureInput input,
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
            clearQueue(access.m_requests);
            access.m_requests.activeMetadataToken = {};
            access.m_requests.activeFrameToken = {};
            access.m_requests.activeFrameRefinement = false;
            access.m_requests.hasLastFrameDemand = false;
            access.m_requests.lastFrameDemand = {};
            access.m_requests.nextRequestToken = 0;
            return ViewportProviderFrameTransportEffect {};
        });
    if (sessionWasActive && result.changes.requestState
        && !result.providerFrameTransport.closeSession) {
        result.providerFrameTransport = access.closeSession();
    } else if (!sessionWasActive && result.changes.requestState) {
        clearQueue(access.m_requests);
    }
    return result;
}

ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput input,
    ViewportEngineProviderSessionOpenFailureAccess access)
{
    ViewportEngineProviderSessionOpenFailureReduction result;
    clearQueue(access.m_requests);
    access.m_session.sessionActive = false;
    access.m_requests.activeMetadataToken = {};
    access.m_requests.activeFrameToken = {};
    access.m_requests.activeFrameRefinement = false;
    access.m_requests.hasLastFrameDemand = false;
    access.m_requests.lastFrameDemand = {};
    result.changes = access.recordTerminal({ input.role, ImageViewportRequestStatus::Error,
        ImageViewportRequestReason::ProviderFailure, FailureScope::Generation, input.diagnostic,
        result.changes });
    updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
    return result;
}

ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput input, ViewportEngineProviderQueueFailureAccess access)
{
    ViewportEngineProviderQueueFailureReduction result;
    const auto& request = requestForRole(access.m_request, input.role);
    const bool queued = access.m_requests.queuedFrameRequest;
    result.diagnostic
        = { queued, input.role, access.m_requests.queuedFrameGeneration, request.identity.id,
              access.m_requests.queuedFrameRequestId, access.m_requests.queuedFrameTargetKind,
              ProviderSchedulerOperation::FlushQueuedFrameRequest };
    if (!queued || !providerPresent(access.m_request, input.role)) {
        clearQueue(access.m_requests);
        return result;
    }

    const bool playbackOwned = access.m_requests.queuedFrameFromPlayback;
    clearQueue(access.m_requests);
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
