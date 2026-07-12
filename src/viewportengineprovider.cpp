#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "imageviewportproviderfacts_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <cmath>
#include <memory>

namespace {
using namespace ImageViewportInternal;

ProviderGenerationState& providerFor(
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

TargetSpreadRoleTerminalState& terminalForRole(RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.targetSpreadTerminal.secondary
                                                      : request.targetSpreadTerminal.primary;
}

bool roleRequired(const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.roles[0].source.facts.present
                                                    : request.roles[1].sequence;
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
        && display.roles[0].displayedImageSize.isValid();
    return retained ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
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

void stageBuiltInSecondaryPayload(RequestState& request, DisplayState& display)
{
    if (!request.roles[1].sequence || request.roles[1].provider
        || request.roles[1].activeRequest.target.frame < 0) {
        return;
    }
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

const TargetSpreadRoleTerminalState* currentTerminal(
    const RequestState& request, ImageViewport::PageRole role)
{
    const TargetSpreadTerminalState& terminal = request.targetSpreadTerminal;
    if (!terminal.sealed || terminal.generation != request.sequenceGeneration
        || terminal.requestId != request.roles[0].activeRequest.identity.id
        || !roleRequired(request, role)) {
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
    return { ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::ProviderFailure,
        event.diagnostic,
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
    return { ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::ProviderFailure,
        event.diagnostic,
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
    auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.generation != providerAccess().request().sequenceGeneration
        || terminal.requestId != providerAccess().request().roles[0].activeRequest.identity.id) {
        terminal.clear();
        terminal.generation = providerAccess().request().sequenceGeneration;
        terminal.requestId = providerAccess().request().roles[0].activeRequest.identity.id;
    }
    terminal.sealed = true;
    auto& roleTerminal = terminalForRole(providerAccess().request(), role);
    roleTerminal.terminal = true;
    roleTerminal.status = status;
    roleTerminal.reason = reason;
    roleTerminal.failureScope = scope;
    roleTerminal.diagnostic = diagnostic;

    const auto* projected = projectedTerminal(providerAccess().request());
    if (!projected) {
        return;
    }
    const bool diagnosticChanged = providerAccess().request().errorString != projected->diagnostic;
    providerAccess().request().status = projected->status;
    providerAccess().request().reason = projected->reason;
    providerAccess().request().errorString = projected->diagnostic;
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
}

ViewportProviderFrameTransportEffect ViewportEngine::closeProviderSession(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameTransportEffect effect;
    auto& provider = providerFor(providerAccess().roles(), role);
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
    auto& provider = providerFor(providerAccess().roles(), role);
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
    providerAccess().request().status = ImageViewport::RequestStatus::Loading;
    providerAccess().request().reason = projectWaitReason(wait);
    providerAccess().display().status = providerAccess().display().roles[0].displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;

    auto& provider = providerFor(providerAccess().roles(), role);
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
        providerAccess().request().roles[1].activeRequest.identity
            = providerAccess().request().roles[0].activeRequest.identity;
        providerAccess().request().roles[1].activeRequest.target = target;
        providerAccess().request().roles[1].activeRequest.resolvedFrame = { target.frame, position };
        providerAccess().request().roles[1].activeRequest.providerFrameToken = {};
        providerAccess().request().roles[1].activeRequest.preparedPayloadId
            = providerAccess().request().roles[0].activeRequest.preparedPayloadId;
        if (target.providerTargetKind != ProviderRequestTargetKind::Playback && target.frame >= 0) {
            providerAccess().request().roles[1].latestNonPlaybackRequest
                = providerAccess().request().roles[1].activeRequest;
        }
    }
    auto& request = requestForRole(providerAccess().request(), role);
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
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return result;
    }
    auto& provider = providerFor(providerAccess().roles(), role);
    if (provider.metadataReady) {
        providerAccess().display().clearPendingRenderPayload();
        providerAccess().display().clearRenderFailureRetainedDisplay();
        const auto start = startProviderFrameRequest(
            role, requestForRole(providerAccess().request(), role).target, geometry);
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

ViewportProviderFrameQueueFlushResult ViewportEngine::reduceQueuedProviderFrameRequest(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ViewportProviderFrameQueueFlushResult result;
    const auto flush = flushQueuedProviderFrameRequest(role);
    if (!flush.startRequest) {
        return result;
    }
    const auto& request = requestForRole(providerAccess().request(), role);
    const DisplayRequestTarget target { flush.frame, request.target.position, flush.targetKind };
    const auto start = startProviderFrameRequest(role, target, geometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.diagnostics = providerAccess().request().status == ImageViewport::RequestStatus::Error
        && providerAccess().request().reason == ImageViewport::RequestReason::ProviderFailure;
    result.schedule = playbackScheduleEffect();
    return result;
}

std::array<ViewportProviderFrameTransportEffect, 2> ViewportEngine::restageProviderDemands(
    const GeometryInput& geometry)
{
    std::array<ViewportProviderFrameTransportEffect, 2> effects;
    for (const auto role :
        { ImageViewport::PageRole::Primary, ImageViewport::PageRole::Secondary }) {
        auto& provider = providerFor(providerAccess().roles(), role);
        auto& request = requestForRole(providerAccess().request(), role);
        const bool present = role == ImageViewport::PageRole::Primary
            ? providerAccess().request().roles[0].source.facts.provider
            : providerAccess().request().roles[1].sequence && providerAccess().request().roles[1].provider;
        if (!present || !provider.sessionActive || !provider.metadataReady
            || !provider.activeFrameToken.isValid() || request.identity.id == 0
            || request.resolvedFrame.frame < 0) {
            continue;
        }
        const auto cancelled = provider.activeFrameToken;
        provider.activeFrameToken = {};
        request.providerFrameToken = {};
        const auto start = startProviderFrameRequest(role, request.target, geometry);
        auto& effect = effects[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
        effect.cancelToken = cancelled;
        effect.closeSession = start.closeSession;
        effect.sessionClose = start.sessionClose;
        effect.sendCommand = start.sendCommand;
        effect.command = start.command;
    }
    return effects;
}

ViewportProviderMetadataAdmissionResult ViewportEngine::reduceProviderMetadataAdmission(
    ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata)
{
    ViewportProviderMetadataAdmissionResult result;
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return result;
    }

    const auto reject = [this, role, &result](const QString& diagnostic) {
        providerAccess().playback().providerStartPending = false;
        recordProviderTerminal(role, ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::PayloadRejection, FailureScope::Generation, diagnostic,
            result.changes);
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
        result.providerFrameTransport = closeProviderSession(role);
    };

    const auto admission = FramePreparation::admitProviderMetadata(metadata);
    if (!admission.accepted()) {
        reject(admission.diagnostic);
        return result;
    }

    const auto& source = role == ImageViewport::PageRole::Secondary
        ? providerAccess().request().roles[1].source
        : providerAccess().request().roles[0].source;
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
    result.facts
        = { admission.timedMetadata, metadata.timedPlaybackSupport(), metadata.frameSeekSupport(),
              metadata.positionSeekSupport(), admission.logicalSize, admission.timingIntervals,
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
    result.changes.requestState = result.changes.requestState || factsChanges.requestState;
    result.changes.requestRevision = result.changes.requestRevision || factsChanges.requestRevision;
    GeometryInput acceptedGeometry = geometry;
    if (role == ImageViewport::PageRole::Primary) {
        acceptedGeometry.primaryPresent = true;
        acceptedGeometry.primarySize = admission.facts.logicalSize;
    } else {
        acceptedGeometry.secondarySize = admission.facts.logicalSize;
    }
    const auto target = applyProviderMetadataTargetPolicy(role, admission.facts, acceptedGeometry);
    result.changes.requestState = result.changes.requestState || target.changes.requestState;
    result.changes.requestRevision
        = result.changes.requestRevision || target.changes.requestRevision;
    result.changes.displayState = result.changes.displayState || target.changes.displayState;
    result.changes.displayRevision
        = result.changes.displayRevision || target.changes.displayRevision;
    result.changes.diagnostics = result.changes.diagnostics || target.changes.diagnostics;
    result.changes.playbackPhase = result.changes.playbackPhase || target.changes.playbackPhase;
    result.changes.scheduleUpdate = result.changes.scheduleUpdate || target.changes.scheduleUpdate;
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
    auto& provider = providerFor(providerAccess().roles(), role);
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest(role);
        provider.activeFrameToken = {};
        recordProviderTerminal(role, admission.status, admission.reason,
            FailureScope::DisplayRequest, admission.diagnostic, changes);
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, changes);
        return changes;
    }

    const auto oldRequestStatus = providerAccess().request().status;
    const auto oldRequestReason = providerAccess().request().reason;
    const auto oldGeometry = geometryState(geometry);
    const bool diagnosticsChanged = providerAccess().request().clearDiagnostics();
    provider.activeFrameToken = {};

    if (role == ImageViewport::PageRole::Secondary) {
        providerAccess().display().roles[1].pendingRenderPayload = admission.preparedPayload;
        const bool primaryReady = providerAccess().display().roles[0].pendingRenderPayload.commitPending
            && !providerAccess().display().roles[0].pendingRenderPayload.image.isNull();
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
        providerAccess().request().status = ImageViewport::RequestStatus::Loading;
        providerAccess().request().reason = projectWaitReason(wait);
        providerAccess().display().status = retainedDisplayStatus(providerAccess().display());
    } else {
        providerAccess().request().targetSpreadTerminal.clear();
        providerAccess().display().captureRenderFailureRetainedDisplay(
            providerAccess().request().roles[0].source.facts.present);
        providerAccess().display().commitPreparedPayloadIdentity(
            providerAccess().request().roles[0].activeRequest, admission.preparedPayload);
        stageBuiltInSecondaryPayload(providerAccess().request(), providerAccess().display());
        TargetSpreadWaitState wait;
        wait.requiresSecondary = providerAccess().request().roles[1].sequence
            && (providerAccess().request().roles[1].provider
                || providerAccess().request().roles[1].activeRequest.target.frame >= 0);
        if (geometry.itemBounds.isEmpty()) {
            wait.primary.renderWaiting = true;
            if (wait.requiresSecondary && !providerAccess().request().roles[1].provider) {
                wait.secondary.renderWaiting = true;
            }
        } else {
            wait.primary.uploadPending = true;
            if (wait.requiresSecondary && !providerAccess().request().roles[1].provider) {
                wait.secondary.uploadPending = true;
            }
        }
        if (wait.requiresSecondary && providerAccess().request().roles[1].provider
            && providerAccess().display().roles[1].pendingRenderPayload.image.isNull()) {
            wait.secondary.providerWaiting = true;
        }
        providerAccess().request().status = ImageViewport::RequestStatus::Loading;
        providerAccess().request().reason = projectWaitReason(wait);
        providerAccess().display().status = retainedDisplayStatus(providerAccess().display());
        providerAccess().display().roles[0].pendingRenderPayload.commitPending = true;
        if (providerAccess().playback().phase == ImageViewport::PlaybackPhase::Waiting
            && providerAccess().request().status == ImageViewport::RequestStatus::Ready
            && !providerAccess().display().roles[0].pendingRenderPayload.commitPending) {
            setPlaybackPhase(providerAccess().playback().stopWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing,
                changes);
            providerAccess().playback().stopWhenRequestReady = false;
        }
    }

    const auto newGeometry = geometryState(geometry);
    changes.requestRevision
        = oldRequestStatus != providerAccess().request().status || oldRequestReason != providerAccess().request().reason;
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
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return changes;
    }

    auto& provider = providerFor(providerAccess().roles(), role);
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
        auto& request = requestForRole(providerAccess().request(), role);
        request.target.frame = rejection.selectedFrame;
        request.resolvedFrame = { rejection.selectedFrame, -1 };
        if (!rejection.selectedFromPosition) {
            request.target.position = -1;
        }
        providerAccess().playback().position = -1;
    }
    const bool diagnosticsChanged = providerAccess().request().clearDiagnostics();
    if (rejection.clearPlaybackStartPending) {
        providerAccess().playback().providerStartPending = false;
    }
    recordProviderTerminal(
        role, rejection.status, rejection.reason, FailureScope::DisplayRequest, {}, changes);
    setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, changes);
    changes.diagnostics = changes.diagnostics || diagnosticsChanged;
    return changes;
}

ViewportProviderMetadataTargetPolicyResult ViewportEngine::applyProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts,
    const GeometryInput& geometry)
{
    ViewportProviderMetadataTargetPolicyResult result;
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return result;
    }
    auto& provider = providerFor(providerAccess().roles(), role);
    const int frameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    DisplayRequestTarget target;

    if (role == ImageViewport::PageRole::Primary) {
        const auto request = providerAccess().request().roles[0].activeRequest;
        const bool playback = providerAccess().playback().providerStartPending
            && request.target.providerTargetKind == ProviderRequestTargetKind::Playback;
        const bool position
            = request.target.providerTargetKind == ProviderRequestTargetKind::Position;
        target.providerTargetKind = playback ? ProviderRequestTargetKind::Playback
            : position                       ? ProviderRequestTargetKind::Position
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
        const bool carrySecondary = providerAccess().request().roles[1].sequence
            && providerAccess().request().roles[1].provider
            && providerAccess().request().roles[1].activeRequest.identity.id == request.identity.id
            && unknownMetadataInitialRequest(request)
            && unknownMetadataInitialRequest(providerAccess().request().roles[1].activeRequest);
        providerAccess().request().beginDisplayRequest(DisplayRequestOrigin::MetadataBoundSelection, target,
            { target.frame, resolvedPosition },
            target.providerTargetKind != ProviderRequestTargetKind::Playback);
        if (carrySecondary) {
            providerAccess().request().roles[1].activeRequest.identity
                = providerAccess().request().roles[0].activeRequest.identity;
            providerAccess().request().roles[1].activeRequest.preparedPayloadId
                = providerAccess().request().roles[0].activeRequest.preparedPayloadId;
        }
        providerAccess().playback().position = target.position;
        providerAccess().playback().providerStartPending = false;
    } else {
        const auto request = providerAccess().request().roles[1].activeRequest;
        if (request.identity.id == 0
            || request.identity.id != providerAccess().request().roles[0].activeRequest.identity.id) {
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
                target.position
                    = facts.timedMetadata && target.frame >= 0 && target.frame < frameCount
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
        providerAccess().display().clearPendingRenderPayload();
        providerAccess().display().clearRenderFailureRetainedDisplay();
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
    auto& provider = providerFor(providerAccess().roles(), role);
    provider.sessionActive = false;
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
    auto& provider = providerFor(providerAccess().roles(), role);
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].provider;
    if (!providerPresent || !provider.sessionActive) {
        return result;
    }

    const DisplayRequest& request = requestForRole(providerAccess().request(), role);
    const bool frameToken = event.token.isValid() && event.token == provider.activeFrameToken
        && event.token == request.providerFrameToken;
    if (frameToken) {
        const auto terminal = frameTerminal(event);
        clearQueuedProviderFrameRequest(role);
        provider.activeFrameToken = {};
        recordProviderTerminal(role, terminal.status, terminal.reason, FailureScope::DisplayRequest,
            FramePreparation::boundedDiagnostic(terminal.diagnostic, terminal.fallbackDiagnostic),
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
    providerAccess().playback().providerStartPending = false;
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
    auto& provider = providerFor(providerAccess().roles(), role);
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
    auto& provider = providerFor(providerAccess().roles(), role);
    const DisplayRequest& request = requestForRole(providerAccess().request(), role);
    const bool queued = provider.queuedFrameRequest;
    result.diagnostic = { queued, role, provider.queuedFrameGeneration, request.identity.id,
        provider.queuedFrameRequestId, provider.queuedFrameTargetKind,
        ProviderSchedulerOperation::FlushQueuedFrameRequest };
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].provider;
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
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return changes;
    }
    auto& provider = providerFor(providerAccess().roles(), role);
    const bool providerPresent = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].provider;
    if (!providerPresent || !provider.sessionActive
        || (event.progress
            && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
                || event.progressValue > 1.0))) {
        return changes;
    }
    const bool metadataToken = !provider.metadataReady && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const DisplayRequest& request = requestForRole(providerAccess().request(), role);
    const bool frameToken = provider.activeFrameToken.isValid()
        && event.token == provider.activeFrameToken && event.token == request.providerFrameToken;
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
        provider.activeMetadataToken = {};
    }
    if (input.activeFrameToken) {
        provider.activeFrameToken = {};
    }
    providerAccess().playback().providerStartPending = false;
    providerAccess().playback().stopWhenRequestReady = false;
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
    auto& provider = providerFor(providerAccess().roles(), role);
    const bool present = role == ImageViewport::PageRole::Primary
        ? providerAccess().request().roles[0].source.facts.provider
        : providerAccess().request().roles[1].sequence && providerAccess().request().roles[1].provider;
    if (!present || !provider.sessionActive) {
        return result;
    }
    const bool metadataToken = !provider.metadataReady && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const auto& request = requestForRole(providerAccess().request(), role);
    const bool frameToken = provider.activeFrameToken.isValid()
        && event.token == provider.activeFrameToken && event.token == request.providerFrameToken;
    if (!metadataToken && !frameToken) {
        return result;
    }
    const auto& terminal = providerAccess().request().targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == providerAccess().request().sequenceGeneration
        && terminal.requestId == providerAccess().request().roles[0].activeRequest.identity.id) {
        return result;
    }
    if (metadataToken || !provider.metadataReady || !provider.timedMetadata
        || request.target.providerTargetKind != ProviderRequestTargetKind::Playback) {
        return reduceProviderEndOfSequenceProtocolViolation(role, { metadataToken, frameToken });
    }

    provider.activeFrameToken = {};
    const bool diagnosticsChanged = providerAccess().request().clearDiagnostics();
    const bool loop
        = effectiveProviderLooping(providerAccess().playback(), provider.authoredAnimationFacts);
    const int selectedFrame = loop ? 0 : provider.timingIntervals.frameCount() - 1;
    const int selectedPosition
        = loop ? 0 : provider.timingIntervals.frameStartPosition(selectedFrame);
    providerAccess().playback().position = loop ? 0 : provider.timingIntervals.totalDuration();
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
        providerAccess().request().roles[1].activeRequest.resolvedFrame = { selectedFrame, selectedPosition };
        providerAccess().request().roles[1].activeRequest.providerFrameToken = {};
        providerAccess().request().roles[1].activeRequest.preparedPayloadId
            = providerAccess().request().roles[0].activeRequest.preparedPayloadId;
    } else {
        providerAccess().request().roles[0].activeRequest.target = target;
        providerAccess().request().roles[0].activeRequest.resolvedFrame = { selectedFrame, selectedPosition };
    }

    const bool sameDisplayedFinal = role == ImageViewport::PageRole::Primary && !loop
        && providerAccess().display().hasReadyDisplay(providerAccess().request().roles[0].source.facts.present)
        && providerAccess().display().roles[0].displayedRequest.generation == providerAccess().request().sequenceGeneration
        && providerAccess().display().roles[0].displayedRequest.request.resolvedFrame.frame == selectedFrame
        && providerAccess().display().roles[0].displayedRequest.request.resolvedFrame.position
            == selectedPosition;
    if (sameDisplayedFinal) {
        providerAccess().request().status = ImageViewport::RequestStatus::Ready;
        providerAccess().request().reason = ImageViewport::RequestReason::Ready;
        providerAccess().display().status = ImageViewport::DisplayStatus::Ready;
        setPlaybackPhase(ImageViewport::PlaybackPhase::Stopped, result.changes);
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
    const auto start = startProviderFrameRequest(role, target, geometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    if (loop && !providerAccess().playback().looping) {
        ++providerAccess().playback().loopIterationsCompleted;
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
