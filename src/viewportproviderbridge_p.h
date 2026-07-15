#pragma once

#include "imageviewportstate_p.h"
#include "viewportproviderevent_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QSet>
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
    virtual bool queueSessionDestruction(ImageSequenceProviderSession* session) = 0;
    virtual bool releaseFrameHandle(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        ImageSequenceProviderFrameHandle* frameHandle)
        = 0;
};

struct ViewportProviderTransportResult
{
    bool delivered = false;
    ImageViewportInternal::ProviderTransportDiagnostic diagnostic;
};

struct ViewportProviderSessionOpenTransportResult
{
    bool opened = false;
    QString diagnostic;
};

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ImageViewportPageRole role = ImageViewportPageRole::Primary);

    ViewportProviderTransportResult closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    ViewportProviderSessionOpenTransportResult openSession(
        const ViewportProviderSessionOpenInput& input);
    ViewportProviderTransportResult deliverRequest(const ImageSequenceProviderRequest& request);
    void completeFrameEventDelivery(quint64 leaseId);
    void reconcileFrameLeases(const QSet<quint64>& liveLeaseIds);
    void releaseAllFrameLeases();
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
    quint64 claimFrameHandle(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        ImageSequenceProviderFrameHandle* frameHandle);
    bool hasFrameLeases(ImageSequenceProviderSession* session) const;
    void releaseFrameLease(quint64 leaseId);
    void destroyClosingSessionIfUnused(ImageSequenceProviderSession* session);

    struct FrameLeaseRecord
    {
        QPointer<ImageSequenceProviderSession> session;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        ImageSequenceProviderThreadingContract threadingContract
            = ImageSequenceProviderThreadingContract::AffinityBound;
        bool pendingEngineDelivery = true;
    };

    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderThreadingContract activeThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<ImageSequenceProviderSession> activeSession;
    QPointer<ImageSequenceProviderSession> pendingCleanupSession;
    ImageSequenceProviderRequestToken pendingCleanupMetadataToken;
    ImageSequenceProviderRequestToken pendingCleanupFrameToken;
    QHash<quint64, FrameLeaseRecord> frameLeases;
    QSet<ImageSequenceProviderSession*> closingSessions;
    bool forceNextCommandDeliveryFailure = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousEventDelivery = false;
#endif
};

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
