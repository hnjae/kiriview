#include "viewportengine_p.h"

#include "viewportcontrollerprovidercontract_p.h"
#include "imageviewportproviderfacts_p.h"

#include <cmath>

namespace {
using namespace ImageViewportInternal;

ProviderGenerationState& providerForRole(ViewportEngine& engine, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? engine.secondaryProviderState()
                                                      : engine.providerState();
}

const DisplayRequest& requestForRole(
    const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryActiveRequest
                                                      : request.activeRequest;
}

DisplayRequest& requestForRole(RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryActiveRequest
                                                      : request.activeRequest;
}

TargetSpreadRoleTerminalState& terminalForRole(
    RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.targetSpreadTerminal.secondary
                                                      : request.targetSpreadTerminal.primary;
}

bool roleRequired(const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.sequenceSource.facts.present
                                                    : request.secondarySequence;
}

const TargetSpreadRoleTerminalState* currentTerminal(
    const RequestState& request, ImageViewport::PageRole role)
{
    const TargetSpreadTerminalState& terminal = request.targetSpreadTerminal;
    if (!terminal.sealed || terminal.generation != request.sequenceGeneration
        || terminal.requestId != request.activeRequest.identity.id || !roleRequired(request, role)) {
        return nullptr;
    }
    const TargetSpreadRoleTerminalState& roleTerminal
        = role == ImageViewport::PageRole::Secondary ? terminal.secondary : terminal.primary;
    return roleTerminal.terminal ? &roleTerminal : nullptr;
}

const TargetSpreadRoleTerminalState* projectedTerminal(const RequestState& request)
{
    const auto* primary = currentTerminal(request, ImageViewport::PageRole::Primary);
    const auto* secondary = currentTerminal(request, ImageViewport::PageRole::Secondary);
    if (!primary) {
        return secondary;
    }
    if (!secondary || primary->status == secondary->status
        || primary->status == ImageViewport::RequestStatus::Error) {
        return primary;
    }
    return secondary->status == ImageViewport::RequestStatus::Error ? secondary : primary;
}

bool unsupportedCauseValid(ImageSequenceProviderSession::UnsupportedCause cause)
{
    return cause == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
        || cause == ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
}

bool invalidUnsupportedCause(const ViewportProviderTerminalEvent& event)
{
    return event.kind == ViewportProviderTerminalEvent::Kind::Unsupported
        && event.unsupportedCauseExplicit && !unsupportedCauseValid(event.unsupportedCause);
}

ViewportProviderFrameTerminalResult frameTerminal(const ViewportProviderTerminalEvent& event)
{
    if (event.kind == ViewportProviderTerminalEvent::Kind::Unsupported) {
        if (invalidUnsupportedCause(event)) {
            return { ImageViewport::RequestStatus::Error,
                ImageViewport::RequestReason::PayloadRejection, {},
                QStringLiteral("provider protocol violation") };
        }
        return { ImageViewport::RequestStatus::Unsupported,
            event.unsupportedCause
                    == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
                ? ImageViewport::RequestReason::UnsupportedRequest
                : ImageViewport::RequestReason::PayloadRejection,
            event.diagnostic, QStringLiteral("provider unsupported") };
    }
    return { ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::ProviderFailure, event.diagnostic,
        event.kind == ViewportProviderTerminalEvent::Kind::Cancellation
            ? QStringLiteral("provider cancelled request")
            : QStringLiteral("provider failure") };
}

ViewportProviderMetadataTerminalResult metadataTerminal(const ViewportProviderTerminalEvent& event)
{
    if (event.kind == ViewportProviderTerminalEvent::Kind::Unsupported) {
        if (invalidUnsupportedCause(event)) {
            return { ImageViewport::RequestStatus::Error,
                ImageViewport::RequestReason::PayloadRejection, {},
                QStringLiteral("provider protocol violation") };
        }
        return { ImageViewport::RequestStatus::Unsupported,
            event.unsupportedCauseExplicit
                    && event.unsupportedCause
                        == ImageSequenceProviderSession::UnsupportedCause::PayloadRejection
                ? ImageViewport::RequestReason::PayloadRejection
                : ImageViewport::RequestReason::UnsupportedRequest,
            event.diagnostic, QStringLiteral("provider unsupported") };
    }
    return { ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::ProviderFailure, event.diagnostic,
        event.kind == ViewportProviderTerminalEvent::Kind::Cancellation
            ? QStringLiteral("provider cancelled request")
            : QStringLiteral("provider failure") };
}
}

void ViewportEngine::recordProviderTerminal(ImageViewport::PageRole role,
    ImageViewport::RequestStatus status, ImageViewport::RequestReason reason,
    ImageViewportInternal::FailureScope scope, const QString& diagnostic,
    ImageViewportInternal::ViewportChangeSet& changes)
{
    auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.generation != m_requestState.sequenceGeneration
        || terminal.requestId != m_requestState.activeRequest.identity.id) {
        terminal.clear();
        terminal.generation = m_requestState.sequenceGeneration;
        terminal.requestId = m_requestState.activeRequest.identity.id;
    }
    terminal.sealed = true;
    auto& roleTerminal = terminalForRole(m_requestState, role);
    roleTerminal.terminal = true;
    roleTerminal.status = status;
    roleTerminal.reason = reason;
    roleTerminal.failureScope = scope;
    roleTerminal.diagnostic = diagnostic;

    const auto* projected = projectedTerminal(m_requestState);
    if (!projected) {
        return;
    }
    const bool diagnosticChanged = m_requestState.errorString != projected->diagnostic;
    m_requestState.status = projected->status;
    m_requestState.reason = projected->reason;
    m_requestState.errorString = projected->diagnostic;
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
}

ViewportProviderFrameTransportEffect ViewportEngine::closeProviderSession(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameTransportEffect effect;
    auto& provider = providerForRole(*this, role);
    effect.closeSession = provider.sessionActive;
    clearQueuedProviderFrameRequest(role);
    if (!provider.sessionActive) {
        return effect;
    }
    effect.sessionClose.metadataToken = provider.activeMetadataToken;
    effect.sessionClose.frameToken = provider.activeFrameToken;
    provider.sessionActive = false;
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    provider.nextRequestToken = 0;
    return effect;
}

ViewportProviderMetadataRequestStartResult ViewportEngine::startProviderMetadataRequest(
    ImageViewport::PageRole role)
{
    ViewportProviderMetadataRequestStartResult result;
    auto& provider = providerForRole(*this, role);
    const auto allocation = allocateProviderRequestToken(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.activeMetadataToken = allocation.token;
    result.sendCommand = provider.sessionActive && provider.activeMetadataToken.isValid();
    result.token = provider.activeMetadataToken;
    return result;
}

ViewportProviderFrameRequestStartResult ViewportEngine::startProviderFrameRequest(
    ImageViewport::PageRole role, DisplayRequestTarget target, const GeometryInput& geometry)
{
    ViewportProviderFrameRequestStartResult result;
    clearQueuedProviderFrameRequest(role);
    TargetSpreadWaitState wait;
    if (role == ImageViewport::PageRole::Secondary) {
        wait.requiresSecondary = true;
        wait.secondary.providerWaiting = true;
    } else {
        wait.primary.providerWaiting = true;
    }
    m_requestState.status = ImageViewport::RequestStatus::Loading;
    m_requestState.reason = projectWaitReason(wait);
    m_displayState.status = m_displayState.displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;

    auto& provider = providerForRole(*this, role);
    const auto allocation = allocateProviderRequestToken(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.activeFrameToken = allocation.token;
    if (!provider.activeFrameToken.isValid()) {
        return result;
    }
    if (role == ImageViewport::PageRole::Secondary) {
        const int position = provider.timedMetadata
            ? provider.timingIntervals.frameStartPosition(target.frame)
            : -1;
        m_requestState.secondaryActiveRequest.identity = m_requestState.activeRequest.identity;
        m_requestState.secondaryActiveRequest.target = target;
        m_requestState.secondaryActiveRequest.resolvedFrame = { target.frame, position };
        m_requestState.secondaryActiveRequest.providerFrameToken = {};
        m_requestState.secondaryActiveRequest.preparedPayloadId
            = m_requestState.activeRequest.preparedPayloadId;
        if (target.providerTargetKind != ProviderRequestTargetKind::Playback && target.frame >= 0) {
            m_requestState.secondaryLatestNonPlaybackRequest
                = m_requestState.secondaryActiveRequest;
        }
    }
    auto& request = requestForRole(m_requestState, role);
    request.providerFrameToken = provider.activeFrameToken;
    result.accepted = true;
    result.sendCommand = provider.sessionActive;
    result.command.token = provider.activeFrameToken;
    result.command.frame = request.resolvedFrame.frame;
    result.command.position = request.target.position;
    result.command.targetKind = request.target.providerTargetKind;
    result.command.demand = providerDisplayDemand(role, geometry);
    return result;
}

ViewportProviderSessionOpenResult ViewportEngine::reduceProviderSessionOpened(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ViewportProviderSessionOpenResult result;
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return result;
    }
    auto& provider = providerForRole(*this, role);
    if (provider.metadataReady) {
        m_displayState.clearPendingRenderPayload();
        m_displayState.clearRenderFailureRetainedDisplay();
        const auto start
            = startProviderFrameRequest(role, requestForRole(m_requestState, role).target, geometry);
        result.providerFrameTransport.closeSession = start.closeSession;
        result.providerFrameTransport.sessionClose = start.sessionClose;
        result.providerFrameTransport.sendCommand = start.sendCommand;
        result.providerFrameTransport.command = start.command;
        return result;
    }
    const auto start = startProviderMetadataRequest(role);
    result.providerMetadataTransport.closeSession = start.closeSession;
    result.providerMetadataTransport.sessionClose = start.sessionClose;
    result.providerMetadataTransport.sendCommand = start.sendCommand;
    result.providerMetadataTransport.token = start.token;
    return result;
}

ViewportProviderMetadataAdmissionResult ViewportEngine::reduceProviderMetadataAdmission(
    ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata)
{
    ViewportProviderMetadataAdmissionResult result;
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return result;
    }

    const auto reject = [this, role, &result](const QString& diagnostic) {
        m_requestState.providerPlaybackStartPending = false;
        recordProviderTerminal(role, ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::PayloadRejection, FailureScope::Generation,
            diagnostic, result.changes);
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
        result.providerFrameTransport = closeProviderSession(role);
    };

    const auto admission = FramePreparation::admitProviderMetadata(metadata);
    if (!admission.accepted()) {
        reject(admission.diagnostic);
        return result;
    }

    const auto& source = role == ImageViewport::PageRole::Secondary
        ? m_requestState.secondarySequenceSource
        : m_requestState.sequenceSource;
    const auto& facts = source.facts;
    if (providerCapabilityContradictsMetadata(
            facts.providerTimedPlaybackCapability, metadata.timedPlaybackSupport())
        || providerCapabilityContradictsMetadata(
            facts.providerFrameSeekCapability, metadata.frameSeekSupport())
        || providerCapabilityContradictsMetadata(
            facts.providerPositionSeekCapability, metadata.positionSeekSupport())) {
        reject(QStringLiteral("provider metadata contradicts construction-time capabilities"));
        return result;
    }
    if (providerFactsContradictMetadata(facts.providerKnownFacts, metadata)) {
        reject(QStringLiteral("provider metadata contradicts construction-time facts"));
        return result;
    }

    result.accepted = true;
    result.facts = { admission.timedMetadata, metadata.timedPlaybackSupport(),
        metadata.frameSeekSupport(), metadata.positionSeekSupport(), admission.logicalSize,
        admission.timingIntervals,
        metadata.hasAuthoredAnimationFacts() ? metadata.authoredAnimationFacts()
                                             : facts.authoredAnimationFacts };
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::acceptProviderMetadataFacts(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    ViewportChangeSet changes;
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return changes;
    }

    auto& provider = providerForRole(*this, role);
    provider.metadataReady = true;
    provider.timedMetadata = facts.timedMetadata;
    provider.timedPlaybackSupport = facts.timedPlaybackSupport;
    provider.frameSeekSupport = facts.frameSeekSupport;
    provider.positionSeekSupport = facts.positionSeekSupport;
    provider.logicalSize = facts.logicalSize;
    provider.timingIntervals = facts.timingIntervals;
    provider.authoredAnimationFacts = facts.authoredAnimationFacts;
    if (role == ImageViewport::PageRole::Secondary) {
        changes.requestState = true;
        changes.requestRevision = true;
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::reduceProviderSessionOpenFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(role);
    auto& provider = providerForRole(*this, role);
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    recordProviderTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::ProviderFailure, FailureScope::Generation, diagnostic,
        changes);
    setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, changes);
    return changes;
}

ViewportProviderTerminalEventResult ViewportEngine::reduceProviderTerminalEvent(
    ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event)
{
    ViewportProviderTerminalEventResult result;
    auto& provider = providerForRole(*this, role);
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? m_requestState.sequenceSource.facts.provider
        : m_requestState.secondarySequenceIsProvider;
    if (!providerPresent || !provider.sessionActive) {
        return result;
    }

    const DisplayRequest& request = requestForRole(m_requestState, role);
    const bool frameToken = event.token.isValid() && event.token == provider.activeFrameToken
        && event.token == request.providerFrameToken;
    if (frameToken) {
        const auto terminal = frameTerminal(event);
        clearQueuedProviderFrameRequest(role);
        provider.activeFrameToken = {};
        recordProviderTerminal(role, terminal.status, terminal.reason, FailureScope::DisplayRequest,
            FramePreparation::boundedDiagnostic(
                terminal.diagnostic, terminal.fallbackDiagnostic),
            result.changes);
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
        if (invalidUnsupportedCause(event)) {
            result.providerFrameTransport = closeProviderSession(role);
        }
        result.schedule = playbackScheduleEffect();
        return result;
    }

    if (provider.metadataReady || !provider.activeMetadataToken.isValid()
        || event.token != provider.activeMetadataToken) {
        return result;
    }
    const auto terminal = metadataTerminal(event);
    provider.activeMetadataToken = {};
    m_requestState.providerPlaybackStartPending = false;
    recordProviderTerminal(role, terminal.status, terminal.reason, FailureScope::Generation,
        FramePreparation::boundedDiagnostic(terminal.diagnostic, terminal.fallbackDiagnostic),
        result.changes);
    setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = closeProviderSession(role);
    result.schedule = playbackScheduleEffect();
    return result;
}

ViewportProviderTerminalEventResult ViewportEngine::reduceProviderDispatchFailure(
    ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    ViewportProviderTerminalEvent terminal { event.token,
        ViewportProviderTerminalEvent::Kind::Failure,
        ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
        event.diagnostic.isEmpty() ? QStringLiteral("provider command delivery failed")
                                   : event.diagnostic,
        false };
    auto& provider = providerForRole(*this, role);
    const bool sessionWasActive = provider.sessionActive;
    provider.sessionActive = true;
    ViewportProviderTerminalEventResult result = reduceProviderTerminalEvent(role, terminal);
    if (!sessionWasActive) {
        provider.sessionActive = false;
        result.providerFrameTransport.closeSession = false;
    }
    if (result.changes.requestState && !result.providerFrameTransport.closeSession) {
        result.providerFrameTransport = closeProviderSession(role);
    }
    result.schedule = playbackScheduleEffect();
    return result;
}

ViewportProviderSchedulerFailureResult ViewportEngine::reduceProviderQueueSchedulingFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    ViewportProviderSchedulerFailureResult result;
    auto& provider = providerForRole(*this, role);
    const DisplayRequest& request = requestForRole(m_requestState, role);
    const bool queued = provider.queuedFrameRequest;
    result.diagnostic = { queued, role, provider.queuedFrameGeneration, request.identity.id,
        provider.queuedFrameRequestId, provider.queuedFrameTargetKind,
        ProviderSchedulerOperation::FlushQueuedFrameRequest };
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? m_requestState.sequenceSource.facts.provider
        : m_requestState.secondarySequenceIsProvider;
    if (!queued || !providerPresent) {
        clearQueuedProviderFrameRequest(role);
        return result;
    }

    const bool playbackOwned = provider.queuedFrameFromPlayback;
    clearQueuedProviderFrameRequest(role);
    recordProviderTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::ProviderFailure, FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(
            diagnostic, QStringLiteral("provider command delivery failed")),
        result.changes);
    if (playbackOwned) {
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
    }
    result.schedule = playbackScheduleEffect();
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::reduceProviderWaitingEvent(
    ImageViewport::PageRole role, const ViewportProviderWaitingEvent& event)
{
    ViewportChangeSet changes;
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return changes;
    }
    auto& provider = providerForRole(*this, role);
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? m_requestState.sequenceSource.facts.provider
        : m_requestState.secondarySequenceIsProvider;
    if (!providerPresent || !provider.sessionActive
        || (event.progress
            && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
                || event.progressValue > 1.0))) {
        return changes;
    }
    const bool metadataToken = !provider.metadataReady && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const DisplayRequest& request = requestForRole(m_requestState, role);
    const bool frameToken = provider.activeFrameToken.isValid()
        && event.token == provider.activeFrameToken && event.token == request.providerFrameToken;
    if ((!metadataToken && !frameToken)
        || m_requestState.status != ImageViewport::RequestStatus::Loading
        || m_requestState.reason == ImageViewport::RequestReason::ProviderWaiting) {
        return changes;
    }
    m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
    changes.requestState = true;
    changes.requestRevision = true;
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportEngine::reduceProviderEndOfSequenceProtocolViolation(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation input)
{
    ViewportProviderEndOfSequenceResult result;
    clearQueuedProviderFrameRequest(role);
    auto& provider = providerForRole(*this, role);
    if (input.activeMetadataToken) {
        provider.activeMetadataToken = {};
    }
    if (input.activeFrameToken) {
        provider.activeFrameToken = {};
    }
    m_requestState.providerPlaybackStartPending = false;
    m_requestState.stopPlaybackWhenRequestReady = false;
    recordProviderTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        input.activeMetadataToken ? FailureScope::Generation : FailureScope::DisplayRequest,
        QStringLiteral("provider protocol violation"), result.changes);
    setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
    result.providerFrameTransport = closeProviderSession(role);
    return result;
}
