#include "viewportengine_p.h"

#include "viewportcontrollerprovidercontract_p.h"
#include "imageviewportproviderfacts_p.h"

#include <cmath>
#include <memory>

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

bool unknownMetadataInitialRequest(const DisplayRequest& request)
{
    return (request.identity.origin == DisplayRequestOrigin::Initial
               || request.identity.origin == DisplayRequestOrigin::StopRestore
               || request.identity.origin == DisplayRequestOrigin::MetadataBoundSelection)
        && request.target.frame < 0 && request.target.position < 0
        && request.target.providerTargetKind == ProviderRequestTargetKind::Unknown;
}

ImageViewport::DisplayStatus retainedDisplayStatus(const DisplayState& display)
{
    const bool retained = (display.status == ImageViewport::DisplayStatus::Ready
                              || display.status == ImageViewport::DisplayStatus::Retained)
        && display.displayedImageSize.isValid();
    return retained ? ImageViewport::DisplayStatus::Retained
                    : ImageViewport::DisplayStatus::Empty;
}

bool effectiveProviderLooping(const RequestState& request,
    const ImageSequenceAuthoredAnimationFacts& authored)
{
    if (request.looping) {
        return true;
    }
    switch (authored.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return request.playbackLoopIterationsCompleted + 1 < authored.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

void stageBuiltInSecondaryPayload(RequestState& request, DisplayState& display)
{
    if (!request.secondarySequence || request.secondarySequenceIsProvider
        || request.secondaryActiveRequest.target.frame < 0) {
        return;
    }
    PreparedPayload payload;
    payload.commitPending = true;
    payload.generation = request.sequenceGeneration;
    payload.requestId = request.activeRequest.identity.id;
    payload.payloadId = ++display.nextPreparedPayloadId;
    request.secondaryActiveRequest.preparedPayloadId = payload.payloadId;
    display.secondaryPendingRenderPayload
        = FramePreparation::admitBuiltInFrame(request.secondarySequenceSource,
              request.secondaryActiveRequest.target.frame, payload)
              .preparedPayload;
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

ViewportProviderTerminalEvent terminalEvent(const ViewportProviderEvent& event)
{
    ViewportProviderTerminalEvent result;
    result.token = event.token;
    result.unsupportedCause = event.unsupportedCause;
    result.diagnostic = event.diagnostic;
    result.unsupportedCauseExplicit = event.unsupportedCauseExplicit;
    result.kind = event.kind == ViewportProviderEvent::Kind::Unsupported
        ? ViewportProviderTerminalEvent::Kind::Unsupported
        : event.kind == ViewportProviderEvent::Kind::Cancellation
        ? ViewportProviderTerminalEvent::Kind::Cancellation
        : ViewportProviderTerminalEvent::Kind::Failure;
    return result;
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

quint64 ViewportEngine::currentProviderGeneration() const
{
    return m_requestState.sequenceGeneration;
}

ViewportProviderFrameQueueFlushResult ViewportEngine::reduceQueuedProviderFrameRequest(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ViewportProviderFrameQueueFlushResult result;
    const auto flush = flushQueuedProviderFrameRequest(role);
    if (!flush.startRequest) {
        return result;
    }
    const auto& request = requestForRole(m_requestState, role);
    const DisplayRequestTarget target {
        flush.frame, request.target.position, flush.targetKind };
    const auto start = startProviderFrameRequest(role, target, geometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.diagnostics = m_requestState.status == ImageViewport::RequestStatus::Error
        && m_requestState.reason == ImageViewport::RequestReason::ProviderFailure;
    result.schedule = playbackScheduleEffect();
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

ViewportProviderMetadataReadyResult ViewportEngine::reduceProviderMetadataReady(
    ImageViewport::PageRole role, const ViewportProviderMetadataReadyEvent& event,
    const GeometryInput& geometry)
{
    ViewportProviderMetadataReadyResult result;
    if (!admitProviderMetadataEvent({ role, event.token }).accepted) {
        return result;
    }
    const auto admission = reduceProviderMetadataAdmission(role, event.metadata);
    if (!admission.accepted) {
        result.changes = admission.changes;
        result.providerFrameTransport = admission.providerFrameTransport;
        return result;
    }
    const auto factsChanges = acceptProviderMetadataFacts(role, admission.facts);
    result.changes.requestState
        = result.changes.requestState || factsChanges.requestState;
    result.changes.requestRevision
        = result.changes.requestRevision || factsChanges.requestRevision;
    GeometryInput acceptedGeometry = geometry;
    if (role == ImageViewport::PageRole::Primary) {
        acceptedGeometry.primaryPresent = true;
        acceptedGeometry.primarySize = admission.facts.logicalSize;
    } else {
        acceptedGeometry.secondarySize = admission.facts.logicalSize;
    }
    const auto target
        = applyProviderMetadataTargetPolicy(role, admission.facts, acceptedGeometry);
    result.changes.requestState = result.changes.requestState || target.changes.requestState;
    result.changes.requestRevision
        = result.changes.requestRevision || target.changes.requestRevision;
    result.changes.displayState = result.changes.displayState || target.changes.displayState;
    result.changes.displayRevision
        = result.changes.displayRevision || target.changes.displayRevision;
    result.changes.diagnostics = result.changes.diagnostics || target.changes.diagnostics;
    result.changes.playbackPhase
        = result.changes.playbackPhase || target.changes.playbackPhase;
    result.changes.scheduleUpdate
        = result.changes.scheduleUpdate || target.changes.scheduleUpdate;
    result.providerFrameTransport = target.providerFrameTransport;
    return result;
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
        const auto metadata
            = reduceProviderMetadataReady(event.role, { event.token, event.metadata }, geometry);
        result.changes = metadata.changes;
        result.providerFrameTransport = metadata.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::BeforeChanges;
        break;
    }
    case ViewportProviderEvent::Kind::ImageFrameReady:
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
        result.changes = reduceProviderFrameEvent(event.role, { event.token }, event.imageFrame,
            event.kind == ViewportProviderEvent::Kind::ImageFrameReady
                ? ImageSequenceProviderFrameMetadata::stillFrame()
                : event.frameMetadata,
            geometry);
        break;
    case ViewportProviderEvent::Kind::FrameHandleReady:
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady: {
        std::unique_ptr<ImageSequenceProviderFrameHandle> owned(event.frameHandle);
        result.changes = reduceProviderFrameEvent(event.role, { event.token },
            owned ? owned->frame() : nullptr,
            event.kind == ViewportProviderEvent::Kind::FrameHandleReady
                ? ImageSequenceProviderFrameMetadata::stillFrame()
                : event.frameMetadata,
            geometry);
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
        const auto terminal = reduceProviderTerminalEvent(event.role, terminalEvent(event));
        result.changes = terminal.changes;
        result.providerFrameTransport = terminal.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::AfterChanges;
        break;
    }
    }
    result.schedule = playbackScheduleEffect();
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::reduceProviderFrameAdmission(
    ImageViewport::PageRole role, const FramePreparation::ProviderFrameAdmissionResult& admission,
    const GeometryInput& geometry)
{
    ViewportChangeSet changes;
    auto& provider = providerForRole(*this, role);
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest(role);
        provider.activeFrameToken = {};
        recordProviderTerminal(role, admission.status, admission.reason,
            FailureScope::DisplayRequest, admission.diagnostic, changes);
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, changes);
        return changes;
    }

    const auto oldRequestStatus = m_requestState.status;
    const auto oldRequestReason = m_requestState.reason;
    const auto oldGeometry = geometryState(geometry);
    const bool diagnosticsChanged = m_requestState.clearDiagnostics();
    provider.activeFrameToken = {};

    if (role == ImageViewport::PageRole::Secondary) {
        m_displayState.secondaryPendingRenderPayload = admission.preparedPayload;
        const bool primaryReady = m_displayState.pendingRenderPayload.commitPending
            && !m_displayState.pendingRenderPayload.image.isNull();
        TargetSpreadWaitState wait;
        wait.requiresSecondary = true;
        if (primaryReady && geometry.itemBounds.isEmpty()) {
            wait.primary.renderWaiting = true;
            wait.secondary.renderWaiting = true;
        } else if (primaryReady) {
            wait.primary.uploadPending = true;
            wait.secondary.uploadPending = true;
        } else {
            wait.primary.providerWaiting = true;
            wait.secondary.uploadPending = true;
        }
        m_requestState.status = ImageViewport::RequestStatus::Loading;
        m_requestState.reason = projectWaitReason(wait);
        m_displayState.status = retainedDisplayStatus(m_displayState);
    } else {
        m_requestState.targetSpreadTerminal.clear();
        m_displayState.captureRenderFailureRetainedDisplay(m_requestState.sequenceSource.facts.present);
        m_displayState.commitPreparedPayloadIdentity(
            m_requestState.activeRequest, admission.preparedPayload);
        stageBuiltInSecondaryPayload(m_requestState, m_displayState);
        TargetSpreadWaitState wait;
        wait.requiresSecondary = m_requestState.secondarySequence
            && (m_requestState.secondarySequenceIsProvider
                || m_requestState.secondaryActiveRequest.target.frame >= 0);
        if (geometry.itemBounds.isEmpty()) {
            wait.primary.renderWaiting = true;
            if (wait.requiresSecondary && !m_requestState.secondarySequenceIsProvider) {
                wait.secondary.renderWaiting = true;
            }
        } else {
            wait.primary.uploadPending = true;
            if (wait.requiresSecondary && !m_requestState.secondarySequenceIsProvider) {
                wait.secondary.uploadPending = true;
            }
        }
        if (wait.requiresSecondary && m_requestState.secondarySequenceIsProvider
            && m_displayState.secondaryPendingRenderPayload.image.isNull()) {
            wait.secondary.providerWaiting = true;
        }
        m_requestState.status = ImageViewport::RequestStatus::Loading;
        m_requestState.reason = projectWaitReason(wait);
        m_displayState.status = retainedDisplayStatus(m_displayState);
        m_displayState.pendingRenderPayload.commitPending = true;
        if (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Waiting
            && m_requestState.status == ImageViewport::RequestStatus::Ready
            && !m_displayState.pendingRenderPayload.commitPending) {
            setPlaybackPhase(m_requestState.stopPlaybackWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing,
                changes);
            m_requestState.stopPlaybackWhenRequestReady = false;
        }
    }

    const auto newGeometry = geometryState(geometry);
    changes.requestRevision = oldRequestStatus != m_requestState.status
        || oldRequestReason != m_requestState.reason;
    changes.requestState = true;
    changes.displayState = true;
    changes.displayRevision = true;
    changes.geometryState = PresentationGeometry::contentRect(oldGeometry)
            != PresentationGeometry::contentRect(newGeometry)
        || PresentationGeometry::visibleImageRect(oldGeometry)
            != PresentationGeometry::visibleImageRect(newGeometry);
    changes.diagnostics = diagnosticsChanged;
    changes.scheduleUpdate = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::reduceProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata, const GeometryInput& geometry)
{
    const ProviderFrameEventAdmission eventAdmission
        = admitProviderFrameEvent({ role, event.token });
    if (!eventAdmission.accepted) {
        return {};
    }
    const auto frameAdmission
        = FramePreparation::admitProviderFrame(frame, metadata, eventAdmission.preparationState);
    return reduceProviderFrameAdmission(role, frameAdmission, geometry);
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

ImageViewportInternal::ViewportChangeSet ViewportEngine::rejectProviderMetadataTarget(
    ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection)
{
    ViewportChangeSet changes;
    if (rejection.updateActiveTarget) {
        auto& request = requestForRole(m_requestState, role);
        request.target.frame = rejection.selectedFrame;
        request.resolvedFrame = { rejection.selectedFrame, -1 };
        if (!rejection.selectedFromPosition) {
            request.target.position = -1;
        }
        m_requestState.playbackPosition = -1;
    }
    const bool diagnosticsChanged = m_requestState.clearDiagnostics();
    if (rejection.clearPlaybackStartPending) {
        m_requestState.providerPlaybackStartPending = false;
    }
    recordProviderTerminal(role, rejection.status, rejection.reason,
        FailureScope::DisplayRequest, {}, changes);
    setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, changes);
    changes.diagnostics = changes.diagnostics || diagnosticsChanged;
    return changes;
}

ViewportProviderMetadataTargetPolicyResult ViewportEngine::applyProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts,
    const GeometryInput& geometry)
{
    ViewportProviderMetadataTargetPolicyResult result;
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return result;
    }
    auto& provider = providerForRole(*this, role);
    const int frameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    DisplayRequestTarget target;

    if (role == ImageViewport::PageRole::Primary) {
        const auto request = m_requestState.activeRequest;
        const bool playback = m_requestState.providerPlaybackStartPending
            && request.target.providerTargetKind == ProviderRequestTargetKind::Playback;
        const bool position
            = request.target.providerTargetKind == ProviderRequestTargetKind::Position;
        target.providerTargetKind = playback ? ProviderRequestTargetKind::Playback
                                             : position ? ProviderRequestTargetKind::Position
                                                        : ProviderRequestTargetKind::Frame;
        target.frame = request.target.frame >= 0 ? request.target.frame : 0;
        target.position = position ? request.target.position : -1;
        if (playback && (!facts.timedMetadata || !provider.timedPlaybackSupport)) {
            result.changes = rejectProviderMetadataTarget(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true });
            return result;
        }
        if (position) {
            if (!facts.timedMetadata || !provider.positionSeekSupport) {
                result.changes = rejectProviderMetadataTarget(role,
                    { ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::UnsupportedRequest });
                return result;
            }
            target.frame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        }
        if (target.frame < 0 || target.frame >= frameCount) {
            result.changes = rejectProviderMetadataTarget(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, target.frame, true, position });
            return result;
        }
        const int resolvedPosition
            = facts.timedMetadata ? facts.timingIntervals.frameStartPosition(target.frame) : -1;
        if (!position) {
            target.position = resolvedPosition;
        }
        const bool carrySecondary = m_requestState.secondarySequence
            && m_requestState.secondarySequenceIsProvider
            && m_requestState.secondaryActiveRequest.identity.id == request.identity.id
            && unknownMetadataInitialRequest(request)
            && unknownMetadataInitialRequest(m_requestState.secondaryActiveRequest);
        m_requestState.beginDisplayRequest(DisplayRequestOrigin::MetadataBoundSelection, target,
            { target.frame, resolvedPosition },
            target.providerTargetKind != ProviderRequestTargetKind::Playback);
        if (carrySecondary) {
            m_requestState.secondaryActiveRequest.identity = m_requestState.activeRequest.identity;
            m_requestState.secondaryActiveRequest.preparedPayloadId
                = m_requestState.activeRequest.preparedPayloadId;
        }
        m_requestState.playbackPosition = target.position;
        m_requestState.providerPlaybackStartPending = false;
    } else {
        const auto request = m_requestState.secondaryActiveRequest;
        if (request.identity.id == 0
            || request.identity.id != m_requestState.activeRequest.identity.id) {
            return result;
        }
        if (unknownMetadataInitialRequest(request)) {
            target = { 0, facts.timedMetadata ? 0 : -1, ProviderRequestTargetKind::Frame };
        } else {
            target = request.target;
            if (target.providerTargetKind == ProviderRequestTargetKind::Playback) {
                if (!facts.timedMetadata || !provider.timedPlaybackSupport) {
                    result.changes = rejectProviderMetadataTarget(role,
                        { ImageViewport::RequestStatus::Unsupported,
                            ImageViewport::RequestReason::UnsupportedRequest });
                    return result;
                }
                target.frame = std::max(target.frame, 0);
                target.position = target.frame < frameCount
                    ? facts.timingIntervals.frameStartPosition(target.frame)
                    : -1;
            } else if (target.providerTargetKind == ProviderRequestTargetKind::Frame) {
                target.position = facts.timedMetadata
                    && target.frame >= 0 && target.frame < frameCount
                    ? facts.timingIntervals.frameStartPosition(target.frame)
                    : -1;
            } else if (target.providerTargetKind == ProviderRequestTargetKind::Position) {
                if (!facts.timedMetadata || !provider.positionSeekSupport) {
                    result.changes = rejectProviderMetadataTarget(role,
                        { ImageViewport::RequestStatus::Unsupported,
                            ImageViewport::RequestReason::UnsupportedRequest });
                    return result;
                }
                target.frame = facts.timingIntervals.frameIndexForPosition(target.position);
            } else {
                return result;
            }
            if (target.frame < 0 || target.frame >= frameCount) {
                result.changes = rejectProviderMetadataTarget(role,
                    { ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::InvalidRequest });
                return result;
            }
        }
    }

    if (role == ImageViewport::PageRole::Primary) {
        m_displayState.clearPendingRenderPayload();
        m_displayState.clearRenderFailureRetainedDisplay();
    }
    const auto start = startProviderFrameRequest(role, target, geometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    if (!start.accepted) {
        result.changes.diagnostics = true;
    }
    return result;
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

ViewportProviderEndOfSequenceResult ViewportEngine::reduceProviderEndOfSequence(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event,
    const GeometryInput& geometry)
{
    ViewportProviderEndOfSequenceResult result;
    auto& provider = providerForRole(*this, role);
    const bool present = role == ImageViewport::PageRole::Primary
        ? m_requestState.sequenceSource.facts.provider
        : m_requestState.secondarySequence && m_requestState.secondarySequenceIsProvider;
    if (!present || !provider.sessionActive) {
        return result;
    }
    const bool metadataToken = !provider.metadataReady && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const auto& request = requestForRole(m_requestState, role);
    const bool frameToken = provider.activeFrameToken.isValid()
        && event.token == provider.activeFrameToken && event.token == request.providerFrameToken;
    if (!metadataToken && !frameToken) {
        return result;
    }
    const auto& terminal = m_requestState.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id) {
        return result;
    }
    if (metadataToken || !provider.metadataReady || !provider.timedMetadata
        || request.target.providerTargetKind != ProviderRequestTargetKind::Playback) {
        return reduceProviderEndOfSequenceProtocolViolation(
            role, { metadataToken, frameToken });
    }

    provider.activeFrameToken = {};
    const bool diagnosticsChanged = m_requestState.clearDiagnostics();
    const bool loop = effectiveProviderLooping(m_requestState, provider.authoredAnimationFacts);
    const int selectedFrame = loop ? 0 : provider.timingIntervals.frameCount() - 1;
    const int selectedPosition
        = loop ? 0 : provider.timingIntervals.frameStartPosition(selectedFrame);
    m_requestState.playbackPosition
        = loop ? 0 : provider.timingIntervals.totalDuration();
    m_requestState.stopPlaybackWhenRequestReady = !loop;
    const DisplayRequestTarget target {
        selectedFrame, selectedPosition, ProviderRequestTargetKind::Playback };

    if (role == ImageViewport::PageRole::Secondary) {
        const auto primary = m_requestState.activeRequest;
        m_requestState.beginDisplayRequest(
            DisplayRequestOrigin::Playback, primary.target, primary.resolvedFrame, false);
        m_requestState.secondaryActiveRequest.identity = m_requestState.activeRequest.identity;
        m_requestState.secondaryActiveRequest.target = target;
        m_requestState.secondaryActiveRequest.resolvedFrame
            = { selectedFrame, selectedPosition };
        m_requestState.secondaryActiveRequest.providerFrameToken = {};
        m_requestState.secondaryActiveRequest.preparedPayloadId
            = m_requestState.activeRequest.preparedPayloadId;
    } else {
        m_requestState.activeRequest.target = target;
        m_requestState.activeRequest.resolvedFrame = { selectedFrame, selectedPosition };
    }

    const bool sameDisplayedFinal = role == ImageViewport::PageRole::Primary && !loop
        && m_displayState.hasReadyDisplay(m_requestState.sequenceSource.facts.present)
        && m_displayState.displayedRequest.generation == m_requestState.sequenceGeneration
        && m_displayState.displayedRequest.request.resolvedFrame.frame == selectedFrame
        && m_displayState.displayedRequest.request.resolvedFrame.position == selectedPosition;
    if (sameDisplayedFinal) {
        m_requestState.status = ImageViewport::RequestStatus::Ready;
        m_requestState.reason = ImageViewport::RequestReason::Ready;
        m_displayState.status = ImageViewport::DisplayStatus::Ready;
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
        m_requestState.stopPlaybackWhenRequestReady = false;
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.diagnostics = diagnosticsChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    m_requestState.targetSpreadTerminal.clear();
    m_displayState.clearPendingRenderPayload();
    m_displayState.clearRenderFailureRetainedDisplay();
    const auto start = startProviderFrameRequest(role, target, geometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    if (loop && !m_requestState.looping) {
        ++m_requestState.playbackLoopIterationsCompleted;
    }
    setPlaybackPhase(ImageViewport::PlaybackPhase::Waiting, result.changes);
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.diagnostics = diagnosticsChanged || !start.accepted;
    result.changes.scheduleUpdate = true;
    return result;
}
