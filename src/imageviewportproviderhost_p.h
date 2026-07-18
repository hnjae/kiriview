#pragma once

#include "viewportproviderbridge_p.h"
#include "viewportprovidercontract_p.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <functional>

class QObject;

class ImageViewportProviderHost
{
public:
    using PageRole = ImageViewportPageRole;

    using EventSink = std::function<void(ViewportProviderHostEvent)>;
    using DiagnosticSink = std::function<void(ImageViewportInternal::ProviderTransportDiagnostic)>;

    ImageViewportProviderHost(
        QObject& dispatchContext, EventSink eventSink, DiagnosticSink diagnosticSink);

    void shutdown();
    void applyTransportEffects(const ViewportProviderTransportBatch& effects);
    void completeFrameEventDelivery(quint64 leaseId);
    void reconcileFrameLeases(const QSet<quint64>& liveLeaseIds);
    void drainCleanup();
    void releaseAllFrameLeases();

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest(PageRole role);
    void failNextQueueFlushSchedulingForTest(PageRole role);
    void useSynchronousExecutorForTest();
    void useSynchronousEventDeliveryForTest();
    void useSynchronousQueueFlushSchedulerForTest();
#endif

private:
    void applyFrameTransportEffect(
        const ViewportProviderFrameTransportEffect& effect, PageRole role = PageRole::Primary);
    void handleProviderEvent(const ViewportProviderEvent& event);
    void applyHostEvent(const ViewportProviderHostEvent& event);

    ViewportProviderBridge& bridgeForRole(PageRole role);
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
    ViewportProviderBridge providerBridge;
    ViewportProviderBridge secondaryProviderBridge;
    QTimer cleanupRetryTimer;
    int cleanupRetryDelayIndex = 0;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousQueueFlushScheduler = false;
    bool failNextPrimaryQueueFlushScheduling = false;
    bool failNextSecondaryQueueFlushScheduling = false;
#endif
};
