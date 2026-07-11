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

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    return state.engine.reduceProviderFrameEvent(
        role, event, frame, metadata, acceptedGeometryInput(viewport));
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

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderMetadataTargetPolicy(ImageViewport::PageRole::Primary, facts);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    return state.engine.applyProviderMetadataTargetPolicy(
        role, facts, acceptedGeometryInput(viewport));
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
    return state.engine.reduceProviderEndOfSequence(
        role, event, acceptedGeometryInput(viewport));
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
