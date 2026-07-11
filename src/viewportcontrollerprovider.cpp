#include "viewportcontrollerplaybackhelpers_p.h"
#include "viewportcontrollergeometryhelpers_p.h"

#include <memory>

namespace {
ViewportProviderTerminalEvent terminalEventFor(const ViewportProviderEvent& event)
{
    ViewportProviderTerminalEvent terminalEvent;
    terminalEvent.token = event.token;
    terminalEvent.unsupportedCause = event.unsupportedCause;
    terminalEvent.diagnostic = event.diagnostic;
    terminalEvent.unsupportedCauseExplicit = event.unsupportedCauseExplicit;

    switch (event.kind) {
    case ViewportProviderEvent::Kind::Unsupported:
        terminalEvent.kind = ViewportProviderTerminalEvent::Kind::Unsupported;
        break;
    case ViewportProviderEvent::Kind::Cancellation:
        terminalEvent.kind = ViewportProviderTerminalEvent::Kind::Cancellation;
        break;
    case ViewportProviderEvent::Kind::MetadataReady:
    case ViewportProviderEvent::Kind::ImageFrameReady:
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
    case ViewportProviderEvent::Kind::FrameHandleReady:
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady:
    case ViewportProviderEvent::Kind::Waiting:
    case ViewportProviderEvent::Kind::Progress:
    case ViewportProviderEvent::Kind::EndOfSequence:
    case ViewportProviderEvent::Kind::Failure:
        terminalEvent.kind = ViewportProviderTerminalEvent::Kind::Failure;
        break;
    }

    return terminalEvent;
}

void appendProviderFrameQueueResult(
    ViewportProviderFrameTransportEffect& effect, ViewportProviderFrameQueueResult queue)
{
    effect.cancelToken = queue.cancelToken;
    effect.deferredControllerEvent = queue.deferredControllerEvent;
}

}

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event)
{
    const ViewportEngine::ProviderFrameEventAdmission admission
        = state.engine.admitProviderFrameEvent({ role, event.token });
    return { admission.accepted, admission.preparationState };
}

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ViewportProviderFrameEvent event)
{
    return acceptProviderFrameEvent(ImageViewport::PageRole::Primary, event);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent = acceptProviderFrameEvent(role, event);
    if (!frameEvent.accepted) {
        return {};
    }

    const FramePreparation::ProviderFrameAdmissionResult admission
        = FramePreparation::admitProviderFrame(frame, metadata, frameEvent.preparationState);
    return handleProviderFrameAdmission(role, admission);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    return handleProviderFrameEvent(ImageViewport::PageRole::Primary, event, frame, metadata);
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ViewportProviderMetadataEvent event)
{
    return acceptProviderMetadataEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ImageViewport::PageRole role, ViewportProviderMetadataEvent event)
{
    const ViewportEngine::ProviderMetadataEventAdmission admission
        = state.engine.admitProviderMetadataEvent({ role, event.token });
    return { admission.accepted };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderSessionOpenFailure(
    const QString& diagnostic)
{
    return handleProviderSessionOpenFailure(ImageViewport::PageRole::Primary, diagnostic);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderSessionOpenFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    return state.engine.reduceProviderSessionOpenFailure(role, diagnostic);
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened()
{
    return handleProviderSessionOpened(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened(
    ImageViewport::PageRole role)
{
    return state.engine.reduceProviderSessionOpened(role, acceptedGeometryInput(viewport));
}

quint64 ViewportController::activateProviderSession()
{
    return activateProviderSession(ImageViewport::PageRole::Primary);
}

quint64 ViewportController::activateProviderSession(ImageViewport::PageRole role)
{
    return state.engine.activateProviderSession(role);
}

void ViewportController::retireProviderSession()
{
    retireProviderSession(ImageViewport::PageRole::Primary);
}

void ViewportController::retireProviderSession(ImageViewport::PageRole role)
{
    state.engine.retireProviderSession(role);
}

quint64 ViewportController::currentProviderGeneration() const
{
    return currentProviderGeneration(ImageViewport::PageRole::Primary);
}

quint64 ViewportController::currentProviderGeneration(ImageViewport::PageRole) const
{
    return viewportRequestState(viewport).sequenceGeneration;
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ViewportController::providerSessionFactory(
    ImageViewport::PageRole role) const
{
    const ImageViewportInternal::ImageSequenceSource& source
        = role == ImageViewport::PageRole::Secondary
        ? state.engine.requestState().secondarySequenceSource
        : state.engine.requestState().sequenceSource;
    return source.providerSessionFactory;
}

ImageSequenceProviderThreadingContract ViewportController::providerThreadingContract(
    ImageViewport::PageRole role) const
{
    const ImageViewportInternal::ImageSequenceSource& source
        = role == ImageViewport::PageRole::Secondary
        ? state.engine.requestState().secondarySequenceSource
        : state.engine.requestState().sequenceSource;
    return source.facts.providerThreadingContract;
}

bool ViewportController::acceptsProviderSessionResult(quint64 sessionSerial) const
{
    return acceptsProviderSessionResult(ImageViewport::PageRole::Primary, sessionSerial);
}

bool ViewportController::acceptsProviderSessionResult(
    ImageViewport::PageRole role, quint64 sessionSerial) const
{
    const auto& provider = role == ImageViewport::PageRole::Secondary
        ? state.engine.secondaryProviderState()
        : state.engine.providerState();
    return provider.sessionActive && provider.sessionSerial == sessionSerial;
}

bool ViewportController::acceptsProviderSessionResult(
    ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const
{
    return state.engine.acceptsProviderSessionEvent(role, sessionSerial, generation);
}

ViewportProviderEventResult ViewportController::handleProviderEvent(
    const ViewportProviderEvent& event)
{
    if (!acceptsProviderSessionResult(event.role, event.sessionSerial, event.generation)) {
        std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame(event.frameHandle);
        return {};
    }

    ViewportProviderEventResult result;
    switch (event.kind) {
    case ViewportProviderEvent::Kind::MetadataReady: {
        const ViewportProviderMetadataReadyResult metadataResult
            = handleProviderMetadataReadyEvent(event.role, { event.token, event.metadata });
        result.changes = metadataResult.changes;
        result.providerFrameTransport = metadataResult.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::BeforeChanges;
        break;
    }
    case ViewportProviderEvent::Kind::ImageFrameReady:
        result.changes = handleProviderFrameEvent(event.role, { event.token }, event.imageFrame,
            ImageSequenceProviderFrameMetadata::stillFrame());
        break;
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
        result.changes = handleProviderFrameEvent(
            event.role, { event.token }, event.imageFrame, event.frameMetadata);
        break;
    case ViewportProviderEvent::Kind::FrameHandleReady: {
        std::unique_ptr<ImageSequenceProviderFrameHandle> ownedFrame(event.frameHandle);
        result.changes = handleProviderFrameEvent(event.role, { event.token },
            ownedFrame ? ownedFrame->frame() : nullptr,
            ImageSequenceProviderFrameMetadata::stillFrame());
        break;
    }
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady: {
        std::unique_ptr<ImageSequenceProviderFrameHandle> ownedFrame(event.frameHandle);
        result.changes = handleProviderFrameEvent(event.role, { event.token },
            ownedFrame ? ownedFrame->frame() : nullptr, event.frameMetadata);
        break;
    }
    case ViewportProviderEvent::Kind::Waiting:
        result.changes = handleProviderWaitingEvent(event.role, { event.token, false, 0.0 });
        break;
    case ViewportProviderEvent::Kind::Progress:
        result.changes
            = handleProviderWaitingEvent(event.role, { event.token, true, event.progress });
        break;
    case ViewportProviderEvent::Kind::EndOfSequence: {
        const ViewportProviderEndOfSequenceResult endOfSequence
            = handleProviderEndOfSequenceEvent(event.role, { event.token });
        result.changes = endOfSequence.changes;
        result.providerFrameTransport = endOfSequence.providerFrameTransport;
        result.providerFrameTransportPhase = endOfSequence.providerFrameTransport.closeSession
            ? ViewportProviderEventTransportPhase::AfterChanges
            : ViewportProviderEventTransportPhase::BeforeChanges;
        break;
    }
    case ViewportProviderEvent::Kind::Failure:
    case ViewportProviderEvent::Kind::Unsupported:
    case ViewportProviderEvent::Kind::Cancellation: {
        const ViewportProviderTerminalEventResult terminal
            = handleProviderTerminalEvent(event.role, terminalEventFor(event));
        result.changes = terminal.changes;
        result.providerFrameTransport = terminal.providerFrameTransport;
        result.providerFrameTransportPhase = ViewportProviderEventTransportPhase::AfterChanges;
        break;
    }
    }

    result.schedule = state.engine.playbackScheduleEffect();
    return result;
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    const ImageSequenceProviderMetadata& metadata)
{
    return handleProviderMetadataAdmission(ImageViewport::PageRole::Primary, metadata);
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata)
{
    return state.engine.reduceProviderMetadataAdmission(role, metadata);
}

ViewportProviderMetadataReadyResult ViewportController::handleProviderMetadataReadyEvent(
    ImageViewport::PageRole role, const ViewportProviderMetadataReadyEvent& event)
{
    const ViewportProviderMetadataEventAcceptance metadataEvent
        = acceptProviderMetadataEvent(role, { event.token });
    if (!metadataEvent.accepted) {
        return {};
    }

    ViewportProviderMetadataReadyResult result;
    const ViewportProviderMetadataAdmissionResult admission
        = handleProviderMetadataAdmission(role, event.metadata);
    if (!admission.accepted) {
        result.changes = admission.changes;
        result.providerFrameTransport = admission.providerFrameTransport;
        return result;
    }

    mergeChanges(result.changes, handleProviderAcceptedMetadataFacts(role, admission.facts));
    const ViewportProviderMetadataTargetPolicyResult targetPolicy
        = handleProviderMetadataTargetPolicy(role, admission.facts);
    mergeChanges(result.changes, targetPolicy.changes);
    result.providerFrameTransport = targetPolicy.providerFrameTransport;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    return handleProviderFrameAdmission(ImageViewport::PageRole::Primary, admission);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    ImageViewport::PageRole role, const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!admission.accepted()) {
        state.engine.clearQueuedProviderFrameRequest(role);
        provider.activeFrameToken = {};
        recordTargetSpreadTerminal(role, admission.status, admission.reason,
            ImageViewportInternal::FailureScope::DisplayRequest, admission.diagnostic, changes);
        setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
        return changes;
    }

    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    provider.activeFrameToken = {};
    const RequestStatusSnapshot oldRequestStatus = requestStatusSnapshot(viewport);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    if (role == ImageViewport::PageRole::Secondary) {
        pendingPayloadForRole(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary)
            = admission.preparedPayload;
        const bool primaryPayloadReady
            = viewportDisplayState(viewport).pendingRenderPayload.commitPending
            && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull();
        ImageViewportInternal::TargetSpreadWaitState waitState;
        waitState.requiresSecondary = true;
        if (primaryPayloadReady && viewport.itemBounds().isEmpty()) {
            waitState.primary.renderWaiting = true;
            waitState.secondary.renderWaiting = true;
        } else if (primaryPayloadReady) {
            waitState.primary.uploadPending = true;
            waitState.secondary.uploadPending = true;
        } else {
            waitState.primary.providerWaiting = true;
            waitState.secondary.uploadPending = true;
        }
        publishLoadingWaitState(waitState);
    } else {
        publishAcceptedTargetState(admission.preparedPayload);
        if (hasSecondaryProviderSequence(viewport)
            && viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull()) {
            ImageViewportInternal::TargetSpreadWaitState waitState;
            waitState.requiresSecondary = true;
            if (viewport.itemBounds().isEmpty()) {
                waitState.primary.renderWaiting = true;
            } else {
                waitState.primary.uploadPending = true;
            }
            waitState.secondary.providerWaiting = true;
            publishLoadingWaitState(waitState);
        }
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready
            && !viewportDisplayState(viewport).pendingRenderPayload.commitPending) {
            setPlaybackPhase(changes,
                viewportRequestState(viewport).stopPlaybackWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing);
            viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        }
    }
    changes.requestRevision = requestStatusChanged(viewport, oldRequestStatus);
    changes.requestState = true;
    markDisplayMutation(changes);
    changes.geometryState = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    markDiagnosticsMutation(changes, diagnosticsValueChanged);
    markScheduleUpdate(changes);
    return changes;
}

ViewportProviderTerminalEventResult ViewportController::handleProviderTerminalEvent(
    const ViewportProviderTerminalEvent& event)
{
    return handleProviderTerminalEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderTerminalEventResult ViewportController::handleProviderTerminalEvent(
    ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event)
{
    return state.engine.reduceProviderTerminalEvent(role, event);
}

ViewportProviderTerminalEventResult ViewportController::handleProviderDispatchFailure(
    ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    return state.engine.reduceProviderDispatchFailure(role, event);
}

ViewportProviderSchedulerFailureResult
ViewportController::handleProviderQueueFlushSchedulingFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    return state.engine.reduceProviderQueueSchedulingFailure(role, diagnostic);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    ImageViewport::PageRole role, const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    state.engine.clearQueuedProviderFrameRequest(role);
    providerGenerationStateForRole(state, role).activeFrameToken = {};
    recordTargetSpreadTerminal(role, result.status, result.reason,
        ImageViewportInternal::FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic), changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    return handleProviderFrameTerminalResult(ImageViewport::PageRole::Primary, result);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    ImageViewport::PageRole role, const ViewportProviderMetadataTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    providerGenerationStateForRole(state, role).activeMetadataToken = {};
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(role, result.status, result.reason,
        ImageViewportInternal::FailureScope::Generation,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic), changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    const ViewportProviderMetadataTerminalResult& result)
{
    return handleProviderMetadataTerminalResult(ImageViewport::PageRole::Primary, result);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataContradiction(
    const ViewportProviderMetadataContradiction& contradiction)
{
    return handleProviderMetadataContradiction(ImageViewport::PageRole::Primary, contradiction);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataContradiction(
    ImageViewport::PageRole role, const ViewportProviderMetadataContradiction& contradiction)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, contradiction.diagnostic, changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderMetadataAdmissionRejection(
    const ViewportProviderMetadataAdmissionRejection& rejection)
{
    return handleProviderMetadataAdmissionRejection(ImageViewport::PageRole::Primary, rejection);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderMetadataAdmissionRejection(
    ImageViewport::PageRole role, const ViewportProviderMetadataAdmissionRejection& rejection)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, rejection.diagnostic, changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTargetRejection(
    ViewportProviderMetadataTargetRejection rejection)
{
    return handleProviderMetadataTargetRejection(ImageViewport::PageRole::Primary, rejection);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTargetRejection(
    ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection)
{
    return state.engine.rejectProviderMetadataTarget(role, rejection);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderMetadataTargetPolicy(ImageViewport::PageRole::Primary, facts);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    const int providerFrameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;

    if (role == ImageViewport::PageRole::Primary) {
        const bool selectedFromPlaybackStart
            = viewportRequestState(viewport).providerPlaybackStartPending
            && request.target.providerTargetKind
                == ImageViewportInternal::ProviderRequestTargetKind::Playback;
        const bool selectedFromPosition = request.target.providerTargetKind
            == ImageViewportInternal::ProviderRequestTargetKind::Position;
        ImageViewportInternal::ProviderRequestTargetKind requestTargetKind
            = selectedFromPlaybackStart
            ? ImageViewportInternal::ProviderRequestTargetKind::Playback
            : (selectedFromPosition ? ImageViewportInternal::ProviderRequestTargetKind::Position
                                    : ImageViewportInternal::ProviderRequestTargetKind::Frame);
        int selectedFrame = request.target.frame >= 0 ? request.target.frame : 0;
        if (selectedFromPlaybackStart && (!facts.timedMetadata || !provider.timedPlaybackSupport)) {
            return { handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true }) };
        }
        if (selectedFromPosition) {
            if (!facts.timedMetadata || !provider.positionSeekSupport) {
                return { handleProviderMetadataTargetRejection(role,
                    { ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::UnsupportedRequest, -1, false, false,
                        false }) };
            }
            selectedFrame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        }
        if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
            return { handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, selectedFrame, true,
                    selectedFromPosition, false }) };
        }

        return handleProviderMetadataTargetSelection(
            { requestTargetKind, selectedFrame, selectedFromPosition, facts.timedMetadata });
    }

    ViewportProviderMetadataTargetPolicyResult result;
    const ImageViewportInternal::DisplayRequest& activeTargetRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary);
    const bool currentIdentity
        = request.identity.id != 0 && request.identity.id == activeTargetRequest.identity.id;
    if (!currentIdentity) {
        return result;
    }

    ImageViewportInternal::DisplayRequestTarget target;
    if (isUnknownMetadataInitialRequest(request)) {
        target = {
            0,
            facts.timedMetadata ? 0 : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Frame,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback) {
        if (!facts.timedMetadata || !provider.timedPlaybackSupport) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        int selectedFrame = request.target.frame;
        if (selectedFrame < 0) {
            selectedFrame = 0;
        }
        if (selectedFrame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, selectedFrame, false, false,
                    false });
            return result;
        }
        target = {
            selectedFrame,
            facts.timingIntervals.frameStartPosition(selectedFrame),
            ImageViewportInternal::ProviderRequestTargetKind::Playback,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Frame) {
        if (request.target.frame < 0 || request.target.frame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, request.target.frame, false,
                    false, false });
            return result;
        }
        target = {
            request.target.frame,
            facts.timedMetadata ? facts.timingIntervals.frameStartPosition(request.target.frame)
                                : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Frame,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Position) {
        if (!facts.timedMetadata || !provider.positionSeekSupport) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        const int selectedFrame
            = facts.timingIntervals.frameIndexForPosition(request.target.position);
        if (selectedFrame < 0) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, -1, false, false, false });
            return result;
        }
        target = {
            selectedFrame,
            request.target.position,
            ImageViewportInternal::ProviderRequestTargetKind::Position,
        };
    } else {
        return result;
    }

    const ViewportProviderFrameDispatchResult dispatch
        = dispatchProviderFrameRequest(role, { target });
    result.providerFrameTransport = dispatch.transport;
    markRequestMutation(result.changes);
    if (!dispatch.accepted) {
        markDiagnosticsMutation(result.changes);
    }
    return result;
}

ViewportProviderMetadataTargetPolicyResult
ViewportController::handleProviderMetadataTargetSelection(
    ViewportProviderMetadataTargetSelection selection)
{
    ViewportProviderMetadataTargetPolicyResult result;
    const ImageViewportInternal::DisplayRequest& primaryActiveRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary);
    const ImageViewportInternal::DisplayRequest& secondaryActiveRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary);
    const bool carrySecondaryInitialRequest = hasSecondaryProviderSequence(viewport)
        && secondaryActiveRequest.identity.id == primaryActiveRequest.identity.id
        && isUnknownMetadataInitialRequest(primaryActiveRequest)
        && isUnknownMetadataInitialRequest(secondaryActiveRequest);
    const bool rememberAsLatestNonPlayback
        = selection.targetKind != ImageViewportInternal::ProviderRequestTargetKind::Playback;
    const int selectedPosition = selection.selectedFromPosition
        ? primaryActiveRequest.target.position
        : selection.timedMetadata ? viewport.providerFrameStartPosition(selection.selectedFrame)
                                  : -1;
    const ImageViewportInternal::ResolvedFrameIdentity resolvedFrame {
        selection.selectedFrame,
        selection.timedMetadata ? viewport.providerFrameStartPosition(selection.selectedFrame) : -1,
    };
    beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::MetadataBoundSelection,
        { selection.selectedFrame, selectedPosition, selection.targetKind }, resolvedFrame,
        rememberAsLatestNonPlayback);
    if (carrySecondaryInitialRequest) {
        ImageViewportInternal::DisplayRequest& carriedSecondaryRequest = activeRequestForRole(
            viewportRequestState(viewport), ImageViewport::PageRole::Secondary);
        const ImageViewportInternal::DisplayRequest& activePrimaryRequest = activeRequestForRole(
            viewportRequestState(viewport), ImageViewport::PageRole::Primary);
        carriedSecondaryRequest.identity = activePrimaryRequest.identity;
        carriedSecondaryRequest.preparedPayloadId = activePrimaryRequest.preparedPayloadId;
    }
    viewportRequestState(viewport).playbackPosition = primaryActiveRequest.target.position;
    publishProviderFrameLoadingState();

    viewportRequestState(viewport).providerPlaybackStartPending = false;
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest({ primaryActiveRequest.target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        markProviderDispatchFailure(result.changes);
        return result;
    }

    markRequestMutation(result.changes);
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderAcceptedMetadataFacts(ImageViewport::PageRole::Primary, facts);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    return state.engine.acceptProviderMetadataFacts(role, facts);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ViewportProviderWaitingEvent event)
{
    return handleProviderWaitingEvent(ImageViewport::PageRole::Primary, event);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ImageViewport::PageRole role, ViewportProviderWaitingEvent event)
{
    return state.engine.reduceProviderWaitingEvent(role, event);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ViewportProviderEndOfSequenceEvent event)
{
    return handleProviderEndOfSequenceEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event)
{
    const bool sealed = targetSpreadTerminalSealedForActiveRequest();
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.sessionActive) {
        return {};
    }

    const bool activeMetadataToken = !provider.metadataReady
        && provider.activeMetadataToken.isValid() && event.token == provider.activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    if (sealed) {
        return {};
    }

    if (activeMetadataToken || !provider.metadataReady || !provider.timedMetadata
        || !activeProviderFrameRequestIsPlayback(state, viewport, role)) {
        return state.engine.reduceProviderEndOfSequenceProtocolViolation(
            role, { activeMetadataToken, activeFrameToken });
    }

    return handleProviderPlaybackEndOfSequence(role);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ViewportProviderEndOfSequenceProtocolViolation violation)
{
    return handleProviderEndOfSequenceProtocolViolation(
        ImageViewport::PageRole::Primary, violation);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation violation)
{
    return state.engine.reduceProviderEndOfSequenceProtocolViolation(role, violation).changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence()
{
    return handleProviderPlaybackEndOfSequence(ImageViewport::PageRole::Primary);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence(
    ImageViewport::PageRole role)
{
    ViewportProviderEndOfSequenceResult result;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.activeFrameToken = {};
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();

    const bool loopPlayback
        = effectiveLoopingForPlayback(viewport, provider.authoredAnimationFacts);
    int selectedFrame = 0;
    int selectedPosition = 0;
    if (loopPlayback) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackPosition = 0;
    } else {
        selectedFrame = provider.timingIntervals.frameCount() - 1;
        selectedPosition = provider.timingIntervals.frameStartPosition(selectedFrame);
        viewportRequestState(viewport).playbackPosition = provider.timingIntervals.totalDuration();
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = true;
    }

    const ImageViewportInternal::DisplayRequestTarget providerTarget {
        selectedFrame,
        selectedPosition,
        ImageViewportInternal::ProviderRequestTargetKind::Playback,
    };
    if (role == ImageViewport::PageRole::Secondary) {
        const ImageViewportInternal::DisplayRequest primaryRequest = activeRequestForRole(
            viewportRequestState(viewport), ImageViewport::PageRole::Primary);
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        setSecondaryActiveRequest(providerTarget, { selectedFrame, selectedPosition }, false);
    } else {
        ImageViewportInternal::DisplayRequest& activeRequest
            = activeRequestForRole(viewportRequestState(viewport), role);
        activeRequest.target = providerTarget;
        activeRequest.resolvedFrame = { selectedFrame, selectedPosition };
    }

    if (role == ImageViewport::PageRole::Primary && !loopPlayback && viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
            == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
            == selectedFrame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
            == selectedPosition) {
        publishReadyDisplayState();
        setPlaybackPhase(result.changes, ImageViewport::PlaybackPhase::Stopped);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        markRequestAndDisplayMutation(result.changes);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        markScheduleUpdate(result.changes);
        return result;
    }

    publishProviderFrameLoadingState(role);
    const ViewportProviderFrameDispatchResult dispatch
        = dispatchProviderFrameRequest(role, { providerTarget });
    result.providerFrameTransport = dispatch.transport;
    if (!dispatch.accepted) {
        markProviderDispatchFailure(result.changes);
        return result;
    }
    updateLoopProgressForAcceptedPlaybackTarget(viewport, loopPlayback);
    setPlaybackPhase(result.changes, ImageViewport::PlaybackPhase::Waiting);
    markRequestAndDisplayMutation(result.changes);
    markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
    markScheduleUpdate(result.changes);
    return result;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession()
{
    return state.engine.closeProviderSession(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return state.engine.closeProviderSession(role);
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose()
{
    return handleProviderSessionClose(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose(
    ImageViewport::PageRole role)
{
    return state.engine.closeProviderSession(role).sessionClose;
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken()
{
    return allocateProviderRequestToken(ImageViewport::PageRole::Primary);
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken(
    ImageViewport::PageRole role)
{
    return state.engine.allocateProviderRequestToken(role);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest()
{
    return startProviderMetadataRequest(ImageViewport::PageRole::Primary);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest(
    ImageViewport::PageRole role)
{
    return state.engine.startProviderMetadataRequest(role);
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameRequestStart request)
{
    return state.engine.startProviderFrameRequest(
        role, request.target, acceptedGeometryInput(viewport));
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ViewportProviderFrameQueueRequest request)
{
    return queueProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameQueueRequest request)
{
    ViewportProviderFrameQueueResult result;
    const ViewportEngine::ProviderFrameQueueResult queue
        = state.engine.queueProviderFrameRequest({ role, request.frame, request.targetKind });
    result.cancelToken = queue.cancelToken;
    if (queue.deferredFlush) {
        result.deferredControllerEvent
            = ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest;
    }
    return result;
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest()
{
    return flushQueuedProviderFrameRequest(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameQueueFlush flush;
    const ViewportEngine::ProviderFrameQueueFlushResult engineFlush
        = state.engine.flushQueuedProviderFrameRequest(role);
    flush.startRequest = engineFlush.startRequest;
    flush.frame = engineFlush.frame;
    flush.targetKind = engineFlush.targetKind;
    return flush;
}

ViewportProviderFrameQueueFlushResult ViewportController::flushQueuedProviderFrameRequestEvent()
{
    return flushQueuedProviderFrameRequestEvent(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameQueueFlushResult ViewportController::flushQueuedProviderFrameRequestEvent(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameQueueFlushResult result;
    const ViewportProviderFrameQueueFlush flush = flushQueuedProviderFrameRequest(role);
    if (!flush.startRequest) {
        return result;
    }

    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    const DisplayRequestTarget target {
        flush.frame,
        activeRequest.target.position,
        flush.targetKind,
    };
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest(role, { target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    markRequestMutation(result.changes);
    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::ProviderFailure) {
        markDiagnosticsMutation(result.changes);
    }
    result.schedule = state.engine.playbackScheduleEffect();
    return result;
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    return startProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    return dispatchProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameDispatchResult result;
    if (state.engine.hasActiveProviderFrameToken(role)) {
        result.accepted = true;
        appendProviderFrameQueueResult(result.transport,
            queueProviderFrameRequest(
                role, { request.target.frame, request.target.providerTargetKind }));
        return result;
    }

    const ViewportProviderFrameRequestStartResult start = startProviderFrameRequest(role, request);
    result.accepted = start.accepted;
    appendProviderFrameStartResult(result.transport, start);
    return result;
}
