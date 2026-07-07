#include "imageviewportproviderhost_p.h"

#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

using namespace ImageViewportInternal;

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
    applyFrameTransportEffect(viewport.controller.closeProviderSession());
    applyFrameTransportEffect(
        viewport.controller.closeProviderSession(PageRole::Secondary), PageRole::Secondary);
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
        if (!bridge.requestMetadata(effect.token)) {
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
        recordTransportResult(bridge.cancelRequest(effect.cancelToken));
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
    bool delivered = false;
    if (effect.command.targetKind == ProviderRequestTargetKind::Playback) {
        delivered = bridge.requestPlayback(
            effect.command.token, effect.command.frame, effect.command.position);
    } else if (effect.command.targetKind == ProviderRequestTargetKind::Position) {
        delivered = bridge.requestPosition(
            effect.command.token, effect.command.frame, effect.command.position);
    } else {
        delivered = bridge.requestFrame(effect.command.token, effect.command.frame);
    }
    if (!delivered) {
        handleDispatchFailure(
            role, effect.command.token, QStringLiteral("provider command delivery failed"));
    }
}

QObject* ImageViewportProviderHost::providerCallbackTarget() const { return viewport.q; }

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageViewportProviderHost::providerSessionFactory(PageRole role) const
{
    const ImageSequenceSource& source = role == PageRole::Secondary
        ? viewport.controller.requestState().secondarySequenceSource
        : viewport.controller.requestState().sequenceSource;
    return source.providerSessionFactory;
}

quint64 ImageViewportProviderHost::installProviderSession(
    PageRole role, ImageSequenceProviderSession* session)
{
    return viewport.controller.installProviderSession(role, session);
}

ImageSequenceProviderSession* ImageViewportProviderHost::takeProviderSession(PageRole role)
{
    return viewport.controller.takeProviderSession(role);
}

ImageSequenceProviderSession* ImageViewportProviderHost::currentProviderSession(PageRole role) const
{
    return viewport.controller.currentProviderSession(role);
}

quint64 ImageViewportProviderHost::currentProviderGeneration(PageRole role) const
{
    return viewport.controller.currentProviderGeneration(role);
}

ImageSequenceProviderThreadingContract ImageViewportProviderHost::providerThreadingContract(
    PageRole role) const
{
    const ImageSequenceSource& source = role == PageRole::Secondary
        ? viewport.controller.requestState().secondarySequenceSource
        : viewport.controller.requestState().sequenceSource;
    return source.facts.providerThreadingContract;
}

void ImageViewportProviderHost::handleProviderEvent(const ViewportProviderEvent& event)
{
    const ViewportProviderEventResult result = viewport.controller.handleProviderEvent(event);
    if (result.providerFrameTransportPhase == ViewportProviderEventTransportPhase::BeforeChanges) {
        applyFrameTransportEffect(result.providerFrameTransport, event.role);
    }
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.sync();
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
        viewport.playbackScheduler.sync();
    }
}

void ImageViewportProviderHost::handleDispatchFailure(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    const ViewportProviderTerminalEventResult result
        = viewport.controller.handleProviderDispatchFailure(role, { token, diagnostic });
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.playbackScheduler.sync();
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
        viewport.playbackScheduler.sync();
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
