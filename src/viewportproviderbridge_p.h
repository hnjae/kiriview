#pragma once

#include "imageviewportstate_p.h"
#include "viewportproviderevent_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <QtCore/Qt>

#include <functional>
#include <memory>

class QObject;

enum class ViewportProviderExecutorOutcome {
    Completed,
    Scheduled,
    RetryableFailure,
};

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

    virtual ViewportProviderExecutorOutcome invokeSessionCommand(
        ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract, std::function<void()> command)
        = 0;
    virtual ViewportProviderExecutorOutcome queueSessionClose(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken)
        = 0;
    virtual ViewportProviderExecutorOutcome queueSessionCleanup(
        ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken)
        = 0;
    virtual ViewportProviderExecutorOutcome queueSessionDestruction(
        ImageSequenceProviderSession* session)
        = 0;
    virtual ViewportProviderExecutorOutcome releaseFrameHandle(
        ImageSequenceProviderSession* session,
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

struct ViewportProviderCleanupResult
{
    QVector<ImageViewportInternal::ProviderTransportDiagnostic> diagnostics;
    bool progress = false;
    bool pending = false;
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
    ViewportProviderCleanupResult drainCleanup(bool retryPendingSessions = true);
    bool hasPendingCleanup() const;
    ViewportProviderCleanupResult releaseAllFrameLeases();
    void setExecutor(ViewportProviderExecutor& executor);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest();
    void useSynchronousEventDeliveryForTest();
#endif

private:
    bool takeForcedDeliveryFailureForTest();
    ViewportProviderExecutor& executor() const;
    Qt::ConnectionType eventDeliveryConnectionType() const;
    quint64 claimFrameHandle(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        ImageSequenceProviderFrameHandle* frameHandle, quint64 generation, quint64 sessionSerial);
    bool hasFrameLeases(ImageSequenceProviderSession* session) const;
    ViewportProviderCleanupResult releaseFrameLease(quint64 leaseId);
    void retrySessionCleanup(ViewportProviderCleanupResult& result, bool retryPendingSessions);
    void destroyClosingSessionIfUnused(
        ImageSequenceProviderSession* session, ViewportProviderCleanupResult& result);

    enum class SessionLifecycle {
        Active,
        CleanupPending,
        Closing,
        DestructionPending,
    };

    struct SessionRecord
    {
        QPointer<ImageSequenceProviderSession> session;
        ImageSequenceProviderThreadingContract threadingContract
            = ImageSequenceProviderThreadingContract::AffinityBound;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
        SessionLifecycle lifecycle = SessionLifecycle::Active;
        ImageSequenceProviderRequestToken metadataToken;
        ImageSequenceProviderRequestToken frameToken;
    };

    struct FrameLeaseRecord
    {
        QPointer<ImageSequenceProviderSession> session;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        ImageSequenceProviderThreadingContract threadingContract
            = ImageSequenceProviderThreadingContract::AffinityBound;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
        bool pendingEngineDelivery = true;
    };

    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<ImageSequenceProviderSession> activeSession;
    QHash<ImageSequenceProviderSession*, SessionRecord> sessions;
    QHash<quint64, FrameLeaseRecord> frameLeases;
    QSet<quint64> retiredFrameLeases;
    bool forceNextCommandDeliveryFailure = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousEventDelivery = false;
#endif
};

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
