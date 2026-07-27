// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportproviderhost_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QObject>

#include <array>
#include <chrono>
#include <utility>

using namespace ImageViewportInternal;

namespace {
using namespace std::chrono_literals;

constexpr std::array cleanupRetryDelays { 0ms, 10ms, 50ms, 250ms, 1000ms };

ViewportProviderCleanupResult mergedCleanupResult(
    ViewportProviderCleanupResult primary, const ViewportProviderCleanupResult& secondary)
{
    primary.diagnostics.append(secondary.diagnostics);
    primary.progress = primary.progress || secondary.progress;
    primary.pending = primary.pending || secondary.pending;
    return primary;
}
}

ImageViewportProviderHost::ImageViewportProviderHost(QObject& dispatchContext, EventSink eventSink,
    DiagnosticSink diagnosticSink, DeferredTransportSink deferredTransportSink)
    : dispatchContext(dispatchContext)
    , eventSink(std::move(eventSink))
    , diagnosticSink(std::move(diagnosticSink))
    , deferredTransportSink(std::move(deferredTransportSink))
    , secondaryProviderBridge(PageRole::Secondary)
{
    cleanupRetryTimer.setSingleShot(true);
    QObject::connect(&cleanupRetryTimer, &QTimer::timeout, &cleanupRetryTimer,
        [this]() { retryPendingCleanup(); });
}

void ImageViewportProviderHost::shutdown()
{
    cleanupRetryTimer.stop();
    deferredPrimarySessionOpen.reset();
    deferredSecondarySessionOpen.reset();
    recordCleanupResult(mergedCleanupResult(providerBridge.releaseAllProviderLeases(),
        secondaryProviderBridge.releaseAllProviderLeases()));
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

void ImageViewportProviderHost::completeFailureEventDelivery(quint64 leaseId)
{
    if (leaseId == 0) {
        return;
    }
    providerBridge.completeFailureEventDelivery(leaseId);
    secondaryProviderBridge.completeFailureEventDelivery(leaseId);
}

void ImageViewportProviderHost::reconcileProviderLeases(
    const QSet<quint64>& liveFrameLeaseIds, const QSet<quint64>& liveFailureLeaseIds)
{
    QSet<quint64> liveLeaseIds = liveFrameLeaseIds;
    liveLeaseIds.unite(liveFailureLeaseIds);
    providerBridge.reconcileLeases(liveLeaseIds);
    secondaryProviderBridge.reconcileLeases(liveLeaseIds);
}

void ImageViewportProviderHost::drainCleanup()
{
    const ViewportProviderCleanupResult result = mergedCleanupResult(
        providerBridge.drainCleanup(false), secondaryProviderBridge.drainCleanup(false));
    recordCleanupResult(result);
    resumeDeferredSessionOpen(PageRole::Primary);
    resumeDeferredSessionOpen(PageRole::Secondary);
}

void ImageViewportProviderHost::releaseAllProviderLeases()
{
    recordCleanupResult(mergedCleanupResult(providerBridge.releaseAllProviderLeases(),
        secondaryProviderBridge.releaseAllProviderLeases()));
}

void ImageViewportProviderHost::applyTransportEffects(const ViewportProviderTransportBatch& effects)
{
    for (const auto& effect : effects) {
        ViewportProviderBridge& bridge = bridgeForRole(effect.role);
        switch (effect.kind) {
        case ViewportProviderTransportCommand::Kind::OpenSession:
            openOrDeferSession(effect);
            break;
        case ViewportProviderTransportCommand::Kind::ActivateSession:
            recordTransportResult(bridge.activateSession(effect.generation, effect.sessionSerial));
            break;
        case ViewportProviderTransportCommand::Kind::SendRequest: {
            const auto result = bridge.deliverRequest(effect.request);
            if (!result.delivered && effect.reportDispatchFailure) {
                handleDispatchFailure(effect.role, effect.request.token());
            } else if (!result.delivered) {
                recordTransportResult(result);
            }
            break;
        }
        case ViewportProviderTransportCommand::Kind::CloseSession: {
            revokeDeferredSessionOpen(effect.role, effect.generation, effect.sessionSerial);
            recordTransportResult(bridge.closeSession(effect.generation, effect.sessionSerial,
                effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
            if (bridge.hasPendingCleanup()) {
                scheduleCleanupRetry(true);
            }
            break;
        }
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

std::optional<ViewportProviderTransportCommand>&
ImageViewportProviderHost::deferredSessionOpenForRole(PageRole role)
{
    return role == PageRole::Secondary ? deferredSecondarySessionOpen : deferredPrimarySessionOpen;
}

void ImageViewportProviderHost::openOrDeferSession(const ViewportProviderTransportCommand& command)
{
    std::optional<ViewportProviderTransportCommand>& deferred
        = deferredSessionOpenForRole(command.role);
    deferred.reset();
    ViewportProviderBridge& bridge = bridgeForRole(command.role);
    const ViewportProviderSessionOpenTransportResult result
        = bridge.openSession({ command.sessionFactory, command.threadingContract,
            command.generation, command.sessionSerial, &dispatchContext,
            [this](const ViewportProviderEvent& event) { handleProviderEvent(event); } });
    if (result.outcome == ViewportProviderSessionOpenTransportOutcome::Deferred) {
        deferred = command;
        if (bridge.hasPendingCleanup()) {
            scheduleCleanupRetry(true);
        }
        return;
    }
    publishSessionOpenResult(command.role, command.generation, command.sessionSerial, result);
}

void ImageViewportProviderHost::resumeDeferredSessionOpen(PageRole role)
{
    std::optional<ViewportProviderTransportCommand>& deferred = deferredSessionOpenForRole(role);
    if (!deferred.has_value() || !bridgeForRole(role).canAdmitSession()) {
        return;
    }

    ViewportProviderTransportCommand command = std::move(*deferred);
    deferred.reset();
    if (deferredTransportSink) {
        deferredTransportSink(std::move(command));
    }
}

void ImageViewportProviderHost::revokeDeferredSessionOpen(
    PageRole role, quint64 generation, quint64 sessionSerial)
{
    std::optional<ViewportProviderTransportCommand>& deferred = deferredSessionOpenForRole(role);
    if (!deferred.has_value()) {
        return;
    }
    if ((generation == 0 && sessionSerial == 0)
        || (deferred->generation == generation && deferred->sessionSerial == sessionSerial)) {
        deferred.reset();
    }
}

void ImageViewportProviderHost::publishSessionOpenResult(PageRole role, quint64 generation,
    quint64 sessionSerial, const ViewportProviderSessionOpenTransportResult& result)
{
    if (result.outcome == ViewportProviderSessionOpenTransportOutcome::Deferred) {
        return;
    }

    ViewportProviderHostEvent event;
    event.kind = result.outcome == ViewportProviderSessionOpenTransportOutcome::Opened
        ? ViewportProviderHostEvent::Kind::SessionOpened
        : ViewportProviderHostEvent::Kind::SessionOpenFailed;
    event.role = role;
    event.providerFailureAvailable = result.providerFailureAvailable;
    event.providerCause = result.providerCause;
    event.providerReference = result.providerReference;
    event.providerFailureLeaseId = result.providerFailureLeaseId;
    event.generation = generation;
    event.sessionSerial = sessionSerial;
    applyHostEvent(event);
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
    if (!progress && std::cmp_less(cleanupRetryDelayIndex + 1, cleanupRetryDelays.size())) {
        ++cleanupRetryDelayIndex;
    }
}

void ImageViewportProviderHost::retryPendingCleanup()
{
    const ViewportProviderCleanupResult result = mergedCleanupResult(
        providerBridge.drainCleanup(true), secondaryProviderBridge.drainCleanup(true));
    recordCleanupResult(result);
    resumeDeferredSessionOpen(PageRole::Primary);
    resumeDeferredSessionOpen(PageRole::Secondary);
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
    applyHostEvent({ ViewportProviderHostEvent::Kind::QueueFlushSchedulingFailed, role });
}

void ImageViewportProviderHost::handleDispatchFailure(
    PageRole role, ImageSequenceProviderRequestToken token)
{
    applyHostEvent({ ViewportProviderHostEvent::Kind::DispatchFailed, role, {}, token });
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
