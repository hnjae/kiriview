#include "viewportcontroller_p.h"

#include "viewportprovidertransporteffects_p.h"

ViewportProviderHostEventResult ViewportController::handleProviderHostEvent(
    const ViewportProviderHostEvent& event)
{
    ViewportProviderHostEventResult result;
    switch (event.kind) {
    case ViewportProviderHostEvent::Kind::SessionOpened: {
        const auto opened = engine.reduceProviderSessionOpened(
            event.role, engine.acceptedGeometryInput(itemBounds()));
        appendProviderTransport(result.afterChanges, opened.providerMetadataTransport, event.role);
        appendProviderTransport(result.afterChanges, opened.providerFrameTransport, event.role);
        return result;
    }
    case ViewportProviderHostEvent::Kind::SessionOpenFailed:
        result.changes = engine.reduceProviderSessionOpenFailure(event.role, event.diagnostic);
        result.schedule = engine.playbackScheduleEffect();
        return result;
    case ViewportProviderHostEvent::Kind::ProviderEvent: {
        const auto reduced = engine.reduceProviderEvent(
            event.providerEvent, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        auto& batch = reduced.providerFrameTransportPhase
                == ViewportProviderEventTransportPhase::BeforeChanges
            ? result.beforeChanges
            : result.afterChanges;
        appendProviderTransport(batch, reduced.providerFrameTransport, event.role);
        result.schedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::DispatchFailed: {
        const auto reduced
            = engine.reduceProviderDispatchFailure(event.role, { event.token, event.diagnostic });
        result.changes = reduced.changes;
        appendProviderTransport(result.afterChanges, reduced.providerFrameTransport, event.role);
        result.schedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest: {
        const auto reduced = engine.reduceQueuedProviderFrameRequest(
            event.role, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        appendProviderTransport(result.afterChanges, reduced.providerFrameTransport, event.role);
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

ViewportProviderTransportBatch ViewportController::restageProviderDemands(double devicePixelRatio)
{
    const auto effects = engine.restageProviderDemands(
        engine.acceptedGeometryInput(itemBounds(), devicePixelRatio));
    ViewportProviderTransportBatch result;
    appendProviderTransport(result, effects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result, effects[1], ImageViewport::PageRole::Secondary);
    return result;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return engine.closeProviderSession(role);
}
