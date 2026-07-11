#include "imageviewportproviderhost_p.h"

#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

using namespace ImageViewportInternal;

namespace {
ImageSequenceProviderRequest providerRequestForCommand(
    ImageViewport::PageRole role, const ViewportProviderFrameCommand& command)
{
    if (command.targetKind == ProviderRequestTargetKind::Playback) {
        return ImageSequenceProviderRequest::playback(command.token, role, command.frame,
            command.position, command.demand);
    }
    if (command.targetKind == ProviderRequestTargetKind::Position) {
        return ImageSequenceProviderRequest::position(command.token, role, command.position,
            command.frame, command.demand);
    }
    return ImageSequenceProviderRequest::frame(
        command.token, role, command.frame, command.demand);
}
}

ImageViewportProviderHost::ImageViewportProviderHost(ImageViewportPrivate& viewport)
    : viewport(viewport)
    , secondaryProviderBridge(PageRole::Secondary)
{
}

bool ImageViewportProviderHost::openSession(PageRole role)
{
    const auto binding = viewport.controller.providerSessionBinding(role);
    if (!bridgeForRole(role).openSession({ binding.factory, binding.threadingContract,
            binding.generation, binding.sessionSerial, viewport.q,
            [this](const ViewportProviderEvent& event) { handleProviderEvent(event); } })) {
        applyHostEvent({ ViewportProviderHostEvent::Kind::SessionOpenFailed, role, {}, {},
            QStringLiteral("provider session creation failed") });
        return false;
    }

    applyHostEvent({ ViewportProviderHostEvent::Kind::SessionOpened, role });
    return true;
}

void ImageViewportProviderHost::closeActiveSessions()
{
    ViewportProviderFrameTransportEffect primary = viewport.controller.closeProviderSession();
    primary.closeSession = true;
    applyFrameTransportEffect(primary);
    ViewportProviderFrameTransportEffect secondary
        = viewport.controller.closeProviderSession(PageRole::Secondary);
    secondary.closeSession = true;
    applyFrameTransportEffect(secondary, PageRole::Secondary);
}

void ImageViewportProviderHost::applyMetadataTransportEffect(
    const ViewportProviderMetadataTransportEffect& effect, PageRole role)
{
    ViewportProviderBridge& bridge = bridgeForRole(role);
    if (effect.closeSession) {
        recordTransportResult(
            bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
    }
    if (effect.sendCommand) {
        const ViewportProviderTransportResult result
            = bridge.deliverRequest(ImageSequenceProviderRequest::metadata(effect.token));
        if (!result.delivered) {
            handleDispatchFailure(
                role, effect.token, QStringLiteral("provider command delivery failed"));
        }
    }
}

void ImageViewportProviderHost::applyFrameTransportEffect(
    const ViewportProviderFrameTransportEffect& effect, PageRole role)
{
    ViewportProviderBridge& bridge = bridgeForRole(role);
    if (effect.cancelToken.isValid()) {
        recordTransportResult(
            bridge.deliverRequest(ImageSequenceProviderRequest::cancel({ effect.cancelToken })));
    }
    if (effect.deferredControllerEvent != ViewportProviderDeferredControllerEvent::None) {
        if (!scheduleDeferredControllerEvent(effect.deferredControllerEvent, role)) {
            return;
        }
    }
    if (effect.closeSession) {
        recordTransportResult(
            bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
    }
    if (!effect.sendCommand) {
        return;
    }
    const ViewportProviderTransportResult result
        = bridge.deliverRequest(providerRequestForCommand(role, effect.command));
    if (!result.delivered) {
        handleDispatchFailure(
            role, effect.command.token, QStringLiteral("provider command delivery failed"));
    }
}

void ImageViewportProviderHost::handleProviderEvent(const ViewportProviderEvent& event)
{
    ViewportProviderHostEvent input;
    input.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    input.role = event.role;
    input.providerEvent = event;
    applyHostEvent(input);
}

void ImageViewportProviderHost::applyHostEvent(const ViewportProviderHostEvent& event)
{
    const ViewportProviderHostEventResult result = viewport.controller.handleProviderHostEvent(event);
    if (result.transportPhase == ViewportProviderEventTransportPhase::BeforeChanges) {
        applyMetadataTransportEffect(result.metadataTransport, event.role);
        applyFrameTransportEffect(result.frameTransport, event.role);
    }
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.apply(result.schedule);
    }
    if (result.schedulerDiagnostic.valid) {
        viewport.internalDiagnostics.recordProviderSchedulerFailure(result.schedulerDiagnostic);
    }
    if (result.transportPhase == ViewportProviderEventTransportPhase::AfterChanges) {
        applyMetadataTransportEffect(result.metadataTransport, event.role);
        applyFrameTransportEffect(result.frameTransport, event.role);
    }
}

ViewportProviderBridge& ImageViewportProviderHost::bridgeForRole(PageRole role)
{
    return role == PageRole::Secondary ? secondaryProviderBridge : providerBridge;
}

void ImageViewportProviderHost::recordTransportResult(
    const ViewportProviderTransportResult& result)
{
    viewport.internalDiagnostics.recordProviderCleanupFailure(result.diagnostic);
}

bool ImageViewportProviderHost::scheduleDeferredControllerEvent(
    ViewportProviderDeferredControllerEvent event, PageRole role)
{
    switch (event) {
    case ViewportProviderDeferredControllerEvent::None:
        return true;
    case ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest:
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
        bool& failNextScheduling = role == PageRole::Secondary
            ? failNextSecondaryQueueFlushScheduling
            : failNextPrimaryQueueFlushScheduling;
        if (failNextScheduling) {
            failNextScheduling = false;
            handleQueueFlushSchedulingFailure(role);
            return false;
        }
        if (synchronousQueueFlushScheduler) {
            flushQueuedFrameRequest(role);
            return true;
        }
#endif
        if (!QMetaObject::invokeMethod(viewport.q,
                [this, role]() { flushQueuedFrameRequest(role); },
                Qt::QueuedConnection)) {
            handleQueueFlushSchedulingFailure(role);
            return false;
        }
        return true;
    }
    return true;
}

void ImageViewportProviderHost::handleQueueFlushSchedulingFailure(PageRole role)
{
    applyHostEvent({ ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed, role, {}, {},
        QStringLiteral("provider queued request scheduling failed") });
}

void ImageViewportProviderHost::handleDispatchFailure(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyHostEvent(
        { ViewportProviderHostEvent::Kind::DispatchFailed, role, {}, token, diagnostic });
}

void ImageViewportProviderHost::flushQueuedFrameRequest(PageRole role)
{
    applyHostEvent({ ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest, role });
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportProviderHost::failNextCommandDeliveryForTest(PageRole role)
{
    bridgeForRole(role).failNextCommandDeliveryForTest();
}

void ImageViewportProviderHost::failNextQueueFlushSchedulingForTest(PageRole role)
{
    if (role == PageRole::Secondary) {
        failNextSecondaryQueueFlushScheduling = true;
        return;
    }
    failNextPrimaryQueueFlushScheduling = true;
}

void ImageViewportProviderHost::useSynchronousExecutorForTest()
{
    ViewportProviderExecutor& executor = synchronousViewportProviderExecutorForTest();
    providerBridge.setExecutor(executor);
    secondaryProviderBridge.setExecutor(executor);
}

void ImageViewportProviderHost::useSynchronousEventDeliveryForTest()
{
    providerBridge.useSynchronousEventDeliveryForTest();
    secondaryProviderBridge.useSynchronousEventDeliveryForTest();
}

void ImageViewportProviderHost::useSynchronousQueueFlushSchedulerForTest()
{
    synchronousQueueFlushScheduler = true;
}
#endif
