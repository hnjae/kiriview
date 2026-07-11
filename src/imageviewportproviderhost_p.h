#pragma once

#include "imageviewport.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportproviderbridge_p.h"

class ImageViewportPrivate;

class ImageViewportProviderHost
{
public:
    using PageRole = ImageViewport::PageRole;

    explicit ImageViewportProviderHost(ImageViewportPrivate& viewport);

    bool openSession(PageRole role = PageRole::Primary);
    void closeActiveSessions();
    void applyFrameTransportEffect(
        const ViewportProviderFrameTransportEffect& effect,
        PageRole role = PageRole::Primary);
    void applyTransportEffects(const ViewportProviderTransportBatch& effects);

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
    void recordTransportResult(const ViewportProviderTransportResult& result);
    bool scheduleDeferredControllerEvent(
        ViewportProviderDeferredControllerEvent event, PageRole role);
    void handleQueueFlushSchedulingFailure(PageRole role);
    void handleDispatchFailure(
        PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic);
    void flushQueuedFrameRequest(PageRole role = PageRole::Primary);

    ImageViewportPrivate& viewport;
    ViewportProviderBridge providerBridge;
    ViewportProviderBridge secondaryProviderBridge;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousQueueFlushScheduler = false;
    bool failNextPrimaryQueueFlushScheduling = false;
    bool failNextSecondaryQueueFlushScheduling = false;
#endif
};
