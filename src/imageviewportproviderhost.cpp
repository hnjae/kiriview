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
    , providerBridge(*this)
    , secondaryProviderBridge(*this, PageRole::Secondary)
{
}

bool ImageViewportProviderHost::openSession(PageRole role)
{
    if (!bridgeForRole(role).openSession()) {
        return false;
    }

    const ViewportProviderSessionOpenResult result
        = viewport.controller.handleProviderSessionOpened(role);
    applyMetadataTransportEffect(result.providerMetadataTransport, role);
    applyFrameTransportEffect(result.providerFrameTransport, role);
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

QObject* ImageViewportProviderHost::providerCallbackTarget() const { return viewport.q; }

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageViewportProviderHost::providerSessionFactory(PageRole role) const
{
    return viewport.controller.providerSessionFactory(role);
}

quint64 ImageViewportProviderHost::activateProviderSession(PageRole role)
{
    return viewport.controller.activateProviderSession(role);
}

void ImageViewportProviderHost::retireProviderSession(PageRole role)
{
    viewport.controller.retireProviderSession(role);
}

quint64 ImageViewportProviderHost::currentProviderGeneration(PageRole role) const
{
    return viewport.controller.currentProviderGeneration(role);
}

ImageSequenceProviderThreadingContract ImageViewportProviderHost::providerThreadingContract(
    PageRole role) const
{
    return viewport.controller.providerThreadingContract(role);
}

void ImageViewportProviderHost::handleProviderEvent(const ViewportProviderEvent& event)
{
    const ViewportProviderEventResult result = viewport.controller.handleProviderEvent(event);
    if (result.providerFrameTransportPhase == ViewportProviderEventTransportPhase::BeforeChanges) {
        applyFrameTransportEffect(result.providerFrameTransport, event.role);
    }
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.apply(result.schedule);
    }
    if (result.providerFrameTransportPhase == ViewportProviderEventTransportPhase::AfterChanges) {
        applyFrameTransportEffect(result.providerFrameTransport, event.role);
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
    const ViewportProviderSchedulerFailureResult result
        = viewport.controller.handleProviderQueueFlushSchedulingFailure(
            role, QStringLiteral("provider queued request scheduling failed"));
    viewport.internalDiagnostics.recordProviderSchedulerFailure(result.diagnostic);
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.apply(result.schedule);
    }
}

void ImageViewportProviderHost::handleDispatchFailure(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    const ViewportProviderTerminalEventResult result
        = viewport.controller.handleProviderDispatchFailure(role, { token, diagnostic });
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.apply(result.schedule);
    }
    applyFrameTransportEffect(result.providerFrameTransport, role);
}

void ImageViewportProviderHost::flushQueuedFrameRequest(PageRole role)
{
    const ViewportProviderFrameQueueFlushResult result
        = viewport.controller.flushQueuedProviderFrameRequestEvent(role);
    applyFrameTransportEffect(result.providerFrameTransport, role);
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.apply(result.schedule);
    }
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
