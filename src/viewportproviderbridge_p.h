#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "viewportproviderevent_p.h"

#include <QtCore/QPointer>
#include <QtCore/Qt>

#include <functional>
#include <memory>

class QObject;

struct ViewportProviderSessionOpenInput
{
    std::shared_ptr<ImageSequenceProviderSessionFactory> factory;
    ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
    QObject* callbackTarget = nullptr;
    std::function<void(const ViewportProviderEvent&)> eventSink;
};

class ViewportProviderExecutor
{
public:
    ViewportProviderExecutor() = default;
    ViewportProviderExecutor(const ViewportProviderExecutor&) = delete;
    ViewportProviderExecutor& operator=(const ViewportProviderExecutor&) = delete;
    virtual ~ViewportProviderExecutor() = default;

    virtual bool invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract, std::function<void()> command)
        = 0;
    virtual bool queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken)
        = 0;
};

struct ViewportProviderTransportResult
{
    bool delivered = false;
    ImageViewportInternal::ProviderTransportDiagnostic diagnostic;
};

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary);

    ViewportProviderTransportResult closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    bool openSession(const ViewportProviderSessionOpenInput& input);
    ViewportProviderTransportResult deliverRequest(const ImageSequenceProviderRequest& request);
    void setExecutor(ViewportProviderExecutor& executor);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest();
    void useSynchronousEventDeliveryForTest();
#endif

private:
    ImageSequenceProviderThreadingContract threadingContract() const;
    bool takeForcedDeliveryFailureForTest();
    ViewportProviderExecutor& executor() const;
    Qt::ConnectionType eventDeliveryConnectionType() const;

    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageSequenceProviderThreadingContract activeThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<ImageSequenceProviderSession> activeSession;
    QPointer<ImageSequenceProviderSession> pendingCleanupSession;
    ImageSequenceProviderRequestToken pendingCleanupMetadataToken;
    ImageSequenceProviderRequestToken pendingCleanupFrameToken;
    bool forceNextCommandDeliveryFailure = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousEventDelivery = false;
#endif
};

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
