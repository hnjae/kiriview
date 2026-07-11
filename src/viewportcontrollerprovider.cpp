#include "viewportcontrollerhelpers_p.h"
#include "viewportcontroller_p.h"

namespace {
void appendProviderFrameQueueResult(
    ViewportProviderFrameTransportEffect& effect, ViewportProviderFrameQueueResult queue)
{
    effect.cancelToken = queue.cancelToken;
    effect.deferredControllerEvent = queue.deferredControllerEvent;
}

void appendProviderFrameStartResult(ViewportProviderFrameTransportEffect& effect,
    const ViewportProviderFrameRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.command = start.command;
}

}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    return engine.reduceProviderFrameEvent(
        role, event, frame, metadata, engine.acceptedGeometryInput(itemBounds()));
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
        = engine.admitProviderMetadataEvent({ role, event.token });
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
    return engine.reduceProviderSessionOpenFailure(role, diagnostic);
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened()
{
    return handleProviderSessionOpened(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened(
    ImageViewport::PageRole role)
{
    return engine.reduceProviderSessionOpened(role, engine.acceptedGeometryInput(itemBounds()));
}

quint64 ViewportController::activateProviderSession()
{
    return activateProviderSession(ImageViewport::PageRole::Primary);
}

quint64 ViewportController::activateProviderSession(ImageViewport::PageRole role)
{
    return engine.activateProviderSession(role);
}

void ViewportController::retireProviderSession()
{
    retireProviderSession(ImageViewport::PageRole::Primary);
}

void ViewportController::retireProviderSession(ImageViewport::PageRole role)
{
    engine.retireProviderSession(role);
}

quint64 ViewportController::currentProviderGeneration() const
{
    return currentProviderGeneration(ImageViewport::PageRole::Primary);
}

quint64 ViewportController::currentProviderGeneration(ImageViewport::PageRole) const
{
    return engine.currentProviderGeneration();
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ViewportController::providerSessionFactory(
    ImageViewport::PageRole role) const
{
    return engine.providerSessionBinding(role).factory;
}

ImageSequenceProviderThreadingContract ViewportController::providerThreadingContract(
    ImageViewport::PageRole role) const
{
    return engine.providerSessionBinding(role).threadingContract;
}

ViewportEngine::ProviderSessionBinding ViewportController::providerSessionBinding(
    ImageViewport::PageRole role) const
{
    return engine.providerSessionBinding(role);
}

bool ViewportController::acceptsProviderSessionResult(quint64 sessionSerial) const
{
    return acceptsProviderSessionResult(ImageViewport::PageRole::Primary, sessionSerial);
}

bool ViewportController::acceptsProviderSessionResult(
    ImageViewport::PageRole role, quint64 sessionSerial) const
{
    const auto binding = engine.providerSessionBinding(role);
    return binding.sessionActive && binding.sessionSerial == sessionSerial;
}

bool ViewportController::acceptsProviderSessionResult(
    ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const
{
    return engine.acceptsProviderSessionEvent(role, sessionSerial, generation);
}

ViewportProviderEventResult ViewportController::handleProviderEvent(
    const ViewportProviderEvent& event)
{
    return engine.reduceProviderEvent(event, engine.acceptedGeometryInput(itemBounds()));
}

ViewportProviderHostEventResult ViewportController::handleProviderHostEvent(
    const ViewportProviderHostEvent& event)
{
    ViewportProviderHostEventResult result;
    switch (event.kind) {
    case ViewportProviderHostEvent::Kind::SessionOpened: {
        const auto opened
            = engine.reduceProviderSessionOpened(event.role, engine.acceptedGeometryInput(itemBounds()));
        result.metadataTransport = opened.providerMetadataTransport;
        result.frameTransport = opened.providerFrameTransport;
        return result;
    }
    case ViewportProviderHostEvent::Kind::SessionOpenFailed:
        result.changes = engine.reduceProviderSessionOpenFailure(event.role, event.diagnostic);
        result.schedule = engine.playbackScheduleEffect();
        return result;
    case ViewportProviderHostEvent::Kind::ProviderEvent: {
        const auto reduced
            = engine.reduceProviderEvent(event.providerEvent, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        result.frameTransport = reduced.providerFrameTransport;
        result.transportPhase = reduced.providerFrameTransportPhase;
        result.schedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::DispatchFailed: {
        const auto reduced
            = engine.reduceProviderDispatchFailure(event.role, { event.token, event.diagnostic });
        result.changes = reduced.changes;
        result.frameTransport = reduced.providerFrameTransport;
        result.schedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest: {
        const auto reduced = engine.reduceQueuedProviderFrameRequest(
            event.role, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        result.frameTransport = reduced.providerFrameTransport;
        result.schedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed: {
        const auto reduced
            = engine.reduceProviderQueueSchedulingFailure(event.role, event.diagnostic);
        result.changes = reduced.changes;
        result.schedulerDiagnostic = reduced.diagnostic;
        result.schedule = reduced.schedule;
        return result;
    }
    }
    return result;
}

std::array<ViewportProviderFrameTransportEffect, 2> ViewportController::restageProviderDemands(
    double devicePixelRatio)
{
    return engine.restageProviderDemands(
        engine.acceptedGeometryInput(itemBounds(), devicePixelRatio));
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    const ImageSequenceProviderMetadata& metadata)
{
    return handleProviderMetadataAdmission(ImageViewport::PageRole::Primary, metadata);
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata)
{
    return engine.reduceProviderMetadataAdmission(role, metadata);
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
    return engine.reduceProviderTerminalEvent(role, event);
}

ViewportProviderTerminalEventResult ViewportController::handleProviderDispatchFailure(
    ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    return engine.reduceProviderDispatchFailure(role, event);
}

ViewportProviderSchedulerFailureResult
ViewportController::handleProviderQueueFlushSchedulingFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    return engine.reduceProviderQueueSchedulingFailure(role, diagnostic);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderMetadataTargetPolicy(ImageViewport::PageRole::Primary, facts);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    return engine.applyProviderMetadataTargetPolicy(
        role, facts, engine.acceptedGeometryInput(itemBounds()));
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderAcceptedMetadataFacts(ImageViewport::PageRole::Primary, facts);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    return engine.acceptProviderMetadataFacts(role, facts);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ViewportProviderWaitingEvent event)
{
    return handleProviderWaitingEvent(ImageViewport::PageRole::Primary, event);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ImageViewport::PageRole role, ViewportProviderWaitingEvent event)
{
    return engine.reduceProviderWaitingEvent(role, event);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ViewportProviderEndOfSequenceEvent event)
{
    return handleProviderEndOfSequenceEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event)
{
    return engine.reduceProviderEndOfSequence(
        role, event, engine.acceptedGeometryInput(itemBounds()));
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession()
{
    return engine.closeProviderSession(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return engine.closeProviderSession(role);
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose()
{
    return handleProviderSessionClose(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose(
    ImageViewport::PageRole role)
{
    return engine.closeProviderSession(role).sessionClose;
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken()
{
    return allocateProviderRequestToken(ImageViewport::PageRole::Primary);
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken(
    ImageViewport::PageRole role)
{
    return engine.allocateProviderRequestToken(role);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest()
{
    return startProviderMetadataRequest(ImageViewport::PageRole::Primary);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest(
    ImageViewport::PageRole role)
{
    return engine.startProviderMetadataRequest(role);
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameRequestStart request)
{
    return engine.startProviderFrameRequest(
        role, request.target, engine.acceptedGeometryInput(itemBounds()));
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
        = engine.queueProviderFrameRequest({ role, request.frame, request.targetKind });
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
        = engine.flushQueuedProviderFrameRequest(role);
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
    return engine.reduceQueuedProviderFrameRequest(
        role, engine.acceptedGeometryInput(itemBounds()));
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
    if (engine.hasActiveProviderFrameToken(role)) {
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
