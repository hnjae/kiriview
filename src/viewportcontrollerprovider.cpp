#include "viewportcontroller_p.h"

#include "viewportprovidertransporteffects_p.h"

ViewportControllerTransition ViewportController::handleProviderHostEvent(
    const ViewportProviderHostEvent& event)
{
    ViewportControllerTransition result;
    switch (event.kind) {
    case ViewportProviderHostEvent::Kind::SessionOpened: {
        const auto opened = engine.reduceProviderSessionOpened(
            event.role, engine.acceptedGeometryInput(itemBounds()));
        appendProviderTransport(
            result.providerAfterPublication, opened.providerMetadataTransport, event.role);
        appendProviderTransport(
            result.providerAfterPublication, opened.providerFrameTransport, event.role);
        return result;
    }
    case ViewportProviderHostEvent::Kind::SessionOpenFailed: {
        const auto reduced = engine.reduceProviderSessionOpenFailure(event.role, event.diagnostic);
        result.changes = reduced.changes;
        result.playbackSchedule = reduced.schedule;
        return result;
    }
    case ViewportProviderHostEvent::Kind::ProviderEvent: {
        const auto reduced = engine.reduceProviderEvent(
            event.providerEvent, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        auto& batch = reduced.providerFrameTransportPhase
                == ViewportProviderEventTransportPhase::BeforeChanges
            ? result.providerBeforePublication
            : result.providerAfterPublication;
        appendProviderTransport(batch, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::DispatchFailed: {
        const auto reduced
            = engine.reduceProviderDispatchFailure(event.role, { event.token, event.diagnostic });
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerAfterPublication, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest: {
        const auto reduced = engine.reduceQueuedProviderFrameRequest(
            event.role, engine.acceptedGeometryInput(itemBounds()));
        result.changes = reduced.changes;
        appendProviderTransport(
            result.providerAfterPublication, reduced.providerFrameTransport, event.role);
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    case ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed: {
        const auto reduced
            = engine.reduceProviderQueueSchedulingFailure(event.role, event.diagnostic);
        result.changes = reduced.changes;
        result.providerSchedulerDiagnostic = reduced.diagnostic;
        if (result.changes.playbackPhase) {
            result.playbackSchedule = reduced.schedule;
        }
        return result;
    }
    }
    return result;
}

ViewportControllerTransition ViewportController::handleDevicePixelRatioChanged(
    double devicePixelRatio)
{
    ViewportControllerTransition result;
    result.changes.displayRevision = true;
    result.changes.geometryState = true;
    result.changes.scheduleUpdate = true;
    const auto effects = engine.restageProviderDemands(
        engine.acceptedGeometryInput(itemBounds(), devicePixelRatio));
    appendProviderTransport(
        result.providerAfterPublication, effects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(
        result.providerAfterPublication, effects[1], ImageViewport::PageRole::Secondary);
    return result;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return engine.closeProviderSession(role);
}
