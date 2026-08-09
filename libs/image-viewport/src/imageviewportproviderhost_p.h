/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportproviderbridge_p.h"
#include "viewportprovidercontract_p.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <functional>
#include <optional>

class QObject;

class ImageViewportProviderHost
{
public:
    using PageRole = ImageViewportPageRole;

    using EventSink = std::function<void(ViewportProviderHostEvent)>;
    using DiagnosticSink = std::function<void(ImageViewportInternal::ProviderTransportDiagnostic)>;
    using DeferredTransportSink = std::function<void(ViewportProviderTransportCommand)>;

    ImageViewportProviderHost(QObject& dispatchContext, EventSink eventSink,
        DiagnosticSink diagnosticSink, DeferredTransportSink deferredTransportSink);

    void shutdown();
    void applyTransportEffects(const ViewportProviderTransportBatch& effects);
    void completeFrameEventDelivery(quint64 leaseId);
    void completeFailureEventDelivery(quint64 leaseId);
    void completeProviderEventDelivery(quint64 deliveryId);
    void reconcileProviderLeases(
        const QSet<quint64>& liveFrameLeaseIds, const QSet<quint64>& liveFailureLeaseIds);
    void drainCleanup();
    void releaseAllProviderLeases();

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest(PageRole role);
    void failNextQueueFlushSchedulingForTest(PageRole role);
    void useSynchronousExecutorForTest();
    void useSynchronousEventDeliveryForTest();
    void useSynchronousQueueFlushSchedulerForTest();
#endif

private:
    void handleProviderEvent(const ViewportProviderEvent& event);
    void applyHostEvent(const ViewportProviderHostEvent& event);

    ViewportProviderBridge& bridgeForRole(PageRole role);
    std::optional<ViewportProviderTransportCommand>& deferredSessionOpenForRole(PageRole role);
    void openOrDeferSession(const ViewportProviderTransportCommand& command);
    void resumeDeferredSessionOpen(PageRole role);
    void revokeDeferredSessionOpen(PageRole role, quint64 generation, quint64 sessionSerial);
    void publishSessionOpenResult(PageRole role, quint64 generation, quint64 sessionSerial,
        const ViewportProviderSessionOpenTransportResult& result);
    void recordTransportResult(const ViewportProviderTransportResult& result);
    void recordCleanupResult(const ViewportProviderCleanupResult& result);
    void scheduleCleanupRetry(bool progress);
    void retryPendingCleanup();
    bool scheduleDeferredEngineEvent(ViewportProviderDeferredEngineEvent event, PageRole role);
    void handleQueueFlushSchedulingFailure(PageRole role);
    void handleDispatchFailure(PageRole role, ImageSequenceProviderRequestToken token);
    void flushQueuedFrameRequest(PageRole role = PageRole::Primary);

    QObject& dispatchContext;
    EventSink eventSink;
    DiagnosticSink diagnosticSink;
    DeferredTransportSink deferredTransportSink;
    ViewportProviderBridge providerBridge;
    ViewportProviderBridge secondaryProviderBridge;
    std::optional<ViewportProviderTransportCommand> deferredPrimarySessionOpen;
    std::optional<ViewportProviderTransportCommand> deferredSecondarySessionOpen;
    QTimer cleanupRetryTimer;
    int cleanupRetryDelayIndex = 0;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousQueueFlushScheduler = false;
    bool failNextPrimaryQueueFlushScheduling = false;
    bool failNextSecondaryQueueFlushScheduling = false;
#endif
};
