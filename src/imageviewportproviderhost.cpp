#include "imageviewportproviderhost_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QObject>

#include <array>

using namespace ImageViewportInternal;

namespace {
constexpr std::array<int, 5> cleanupRetryDelays { 0, 10, 50, 250, 1000 };

ViewportProviderCleanupResult mergedCleanupResult(
    ViewportProviderCleanupResult primary, const ViewportProviderCleanupResult& secondary)
{
    primary.diagnostics.append(secondary.diagnostics);
    primary.progress = primary.progress || secondary.progress;
    primary.pending = primary.pending || secondary.pending;
    return primary;
}
}

ImageViewportProviderHost::ImageViewportProviderHost(
    QObject& dispatchContext, EventSink eventSink, DiagnosticSink diagnosticSink)
    : dispatchContext(dispatchContext)
    , eventSink(std::move(eventSink))
    , diagnosticSink(std::move(diagnosticSink))
    , secondaryProviderBridge(PageRole::Secondary)
{
    cleanupRetryTimer.setSingleShot(true);
    QObject::connect(&cleanupRetryTimer, &QTimer::timeout, [this]() { retryPendingCleanup(); });
}

void ImageViewportProviderHost::shutdown()
{
    cleanupRetryTimer.stop();
    recordCleanupResult(mergedCleanupResult(
        providerBridge.releaseAllFrameLeases(), secondaryProviderBridge.releaseAllFrameLeases()));
    recordTransportResult(providerBridge.closeSession({}, {}));
    recordTransportResult(secondaryProviderBridge.closeSession({}, {}));
    retryPendingCleanup();
    cleanupRetryTimer.stop();
}

void ImageViewportProviderHost::completeFrameEventDelivery(quint64 leaseId)
{
    if (leaseId == 0) {
        return;
    }
    providerBridge.completeFrameEventDelivery(leaseId);
    secondaryProviderBridge.completeFrameEventDelivery(leaseId);
}

void ImageViewportProviderHost::reconcileFrameLeases(const QSet<quint64>& liveLeaseIds)
{
    providerBridge.reconcileFrameLeases(liveLeaseIds);
    secondaryProviderBridge.reconcileFrameLeases(liveLeaseIds);
}

void ImageViewportProviderHost::drainCleanup()
{
    recordCleanupResult(mergedCleanupResult(
        providerBridge.drainCleanup(false), secondaryProviderBridge.drainCleanup(false)));
}

void ImageViewportProviderHost::releaseAllFrameLeases()
{
    recordCleanupResult(mergedCleanupResult(
        providerBridge.releaseAllFrameLeases(), secondaryProviderBridge.releaseAllFrameLeases()));
}

void ImageViewportProviderHost::applyFrameTransportEffect(
    const ViewportProviderFrameTransportEffect& effect, PageRole role)
{
    ViewportProviderTransportBatch batch;
    if (effect.cancelToken.isValid()) {
        batch.append({ ViewportProviderTransportCommand::Kind::SendRequest, role,
            ImageSequenceProviderRequest::cancel({ effect.cancelToken }), {},
            ViewportProviderDeferredEngineEvent::None, false });
    }
    if (effect.deferredEngineEvent != ViewportProviderDeferredEngineEvent::None) {
        batch.append({ ViewportProviderTransportCommand::Kind::ScheduleDeferredEvent, role, {}, {},
            effect.deferredEngineEvent });
    }
    if (effect.closeSession) {
        batch.append({ ViewportProviderTransportCommand::Kind::CloseSession, role, {},
            effect.sessionClose });
    }
    if (effect.sendCommand) {
        ImageSequenceProviderRequest request;
        if (effect.command.targetKind == ProviderRequestTargetKind::Playback) {
            request = ImageSequenceProviderRequest::playback(effect.command.token, role,
                effect.command.frame, effect.command.position, effect.command.demand);
        } else if (effect.command.targetKind == ProviderRequestTargetKind::Position) {
            request = ImageSequenceProviderRequest::position(effect.command.token, role,
                effect.command.position, effect.command.frame, effect.command.demand);
        } else {
            request = ImageSequenceProviderRequest::frame(
                effect.command.token, role, effect.command.frame, effect.command.demand);
        }
        batch.append(
            { ViewportProviderTransportCommand::Kind::SendRequest, role, std::move(request) });
    }
    applyTransportEffects(batch);
}

void ImageViewportProviderHost::applyTransportEffects(const ViewportProviderTransportBatch& effects)
{
    for (const auto& effect : effects) {
        ViewportProviderBridge& bridge = bridgeForRole(effect.role);
        switch (effect.kind) {
        case ViewportProviderTransportCommand::Kind::OpenSession: {
            const auto openResult = bridge.openSession({ effect.sessionFactory,
                effect.threadingContract, effect.generation, effect.sessionSerial, &dispatchContext,
                [this](const ViewportProviderEvent& event) { handleProviderEvent(event); } });
            if (!openResult.opened) {
                applyHostEvent(
                    { ViewportProviderHostEvent::Kind::SessionOpenFailed, effect.role, {}, {},
                        openResult.diagnostic.isEmpty()
                            ? QStringLiteral("provider session creation failed")
                            : openResult.diagnostic });
                return;
            } else {
                applyHostEvent({ ViewportProviderHostEvent::Kind::SessionOpened, effect.role });
            }
            break;
        }
        case ViewportProviderTransportCommand::Kind::SendRequest: {
            const auto result = bridge.deliverRequest(effect.request);
            if (!result.delivered && effect.reportDispatchFailure) {
                handleDispatchFailure(effect.role, effect.request.token(),
                    QStringLiteral("provider command delivery failed"));
            } else if (!result.delivered) {
                recordTransportResult(result);
            }
            break;
        }
        case ViewportProviderTransportCommand::Kind::CloseSession:
            recordTransportResult(bridge.closeSession(
                effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
            break;
        case ViewportProviderTransportCommand::Kind::ScheduleDeferredEvent:
            if (!scheduleDeferredEngineEvent(effect.deferredEvent, effect.role)) {
                return;
            }
            break;
        }
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
    if (eventSink) {
        eventSink(event);
    }
}

ViewportProviderBridge& ImageViewportProviderHost::bridgeForRole(PageRole role)
{
    return role == PageRole::Secondary ? secondaryProviderBridge : providerBridge;
}

void ImageViewportProviderHost::recordTransportResult(const ViewportProviderTransportResult& result)
{
    if (diagnosticSink && result.diagnostic.valid) {
        diagnosticSink(result.diagnostic);
    }
    if (result.diagnostic.valid && result.diagnostic.pendingCleanup) {
        scheduleCleanupRetry(false);
    }
}

void ImageViewportProviderHost::recordCleanupResult(const ViewportProviderCleanupResult& result)
{
    if (diagnosticSink) {
        for (const ProviderTransportDiagnostic& diagnostic : result.diagnostics) {
            if (diagnostic.valid) {
                diagnosticSink(diagnostic);
            }
        }
    }
    if (!result.pending) {
        cleanupRetryTimer.stop();
        cleanupRetryDelayIndex = 0;
        return;
    }
    scheduleCleanupRetry(result.progress);
}

void ImageViewportProviderHost::scheduleCleanupRetry(bool progress)
{
    if (progress) {
        cleanupRetryTimer.stop();
        cleanupRetryDelayIndex = 0;
    }
    if (cleanupRetryTimer.isActive()) {
        return;
    }
    cleanupRetryTimer.start(cleanupRetryDelays[size_t(cleanupRetryDelayIndex)]);
    if (!progress && cleanupRetryDelayIndex < int(cleanupRetryDelays.size()) - 1) {
        ++cleanupRetryDelayIndex;
    }
}

void ImageViewportProviderHost::retryPendingCleanup()
{
    recordCleanupResult(mergedCleanupResult(
        providerBridge.drainCleanup(true), secondaryProviderBridge.drainCleanup(true)));
}

bool ImageViewportProviderHost::scheduleDeferredEngineEvent(
    ViewportProviderDeferredEngineEvent event, PageRole role)
{
    switch (event) {
    case ViewportProviderDeferredEngineEvent::None:
        return true;
    case ViewportProviderDeferredEngineEvent::FlushQueuedFrameRequest:
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
        if (!QMetaObject::invokeMethod(
                &dispatchContext, [this, role]() { flushQueuedFrameRequest(role); },
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
