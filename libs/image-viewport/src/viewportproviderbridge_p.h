/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
#include "viewportproviderevent_p.h"
#include <ImageViewport/imagesequenceprovider.h>

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <QtCore/Qt>

#include <functional>
#include <memory>

class QObject;
class ViewportProviderEventEndpoint;
class ViewportProviderLeaseRegistry;
class ViewportProviderSessionCleanupRegistry;

enum class ViewportProviderExecutorOutcome {
    Completed,
    Scheduled,
    RetryableFailure,
};

enum class ViewportProviderCloseDispatchState {
    Pending,
    Queued,
    Completed,
};

class ViewportProviderSessionControl
    : public std::enable_shared_from_this<ViewportProviderSessionControl>
{
public:
    ViewportProviderSessionControl(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract, quint64 generation,
        quint64 sessionSerial);

    ImageSequenceProviderSession* session() const;
    ImageSequenceProviderThreadingContract threadingContract() const;
    quint64 generation() const;
    quint64 sessionSerial() const;
    bool beginEventIngress();
    void claimEventDelivery();
    void claimHandleLease();
    void endEventIngress();
    void completeEventDelivery();
    void completeCloseOnSessionAffinity();
    void completeHandleReleaseOnSessionAffinity();
    void recordCloseDispatch(ViewportProviderExecutorOutcome outcome);
    [[nodiscard]] ViewportProviderCloseDispatchState closeDispatchState() const;
    void markSessionDestroyed();

private:
    void destroySessionIfReadyOnSessionAffinity();
    void scheduleDestructionCheck();

    mutable QMutex mutex;
    ImageSequenceProviderSession* providerSession = nullptr;
    ImageSequenceProviderThreadingContract contract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    quint64 generationIdentity = 0;
    quint64 sessionIdentity = 0;
    qsizetype activeIngressCount = 0;
    qsizetype acceptedDeliveryCount = 0;
    qsizetype handleLeaseCount = 0;
    ViewportProviderCloseDispatchState closeState = ViewportProviderCloseDispatchState::Pending;
    bool destructionStarted = false;
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
    ViewportProviderExecutor(ViewportProviderExecutor&&) = delete;
    ViewportProviderExecutor& operator=(ViewportProviderExecutor&&) = delete;
    virtual ~ViewportProviderExecutor() = default;

    virtual ViewportProviderExecutorOutcome invokeSessionCommand(
        ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract, std::function<void()> command)
        = 0;
    virtual ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken)
        = 0;
    virtual ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle, std::function<void()>&& completion)
        = 0;
    virtual ViewportProviderExecutorOutcome releaseFailureHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFailureHandle* failureHandle, std::function<void()>&& completion);
};

struct ViewportProviderTransportResult
{
    bool delivered = false;
    ImageViewportInternal::ProviderTransportDiagnostic diagnostic;
};

enum class ViewportProviderSessionOpenTransportOutcome {
    Failed,
    Opened,
    Deferred,
};

struct ViewportProviderSessionOpenTransportResult
{
    ViewportProviderSessionOpenTransportOutcome outcome
        = ViewportProviderSessionOpenTransportOutcome::Failed;
    bool providerFailureAvailable = false;
    ImageSequenceProviderFailureCause providerCause
        = ImageSequenceProviderFailureCause::Unavailable;
    ImageSequenceProviderFailureReference providerReference;
    quint64 providerFailureLeaseId = 0;

    [[nodiscard]] bool isOpened() const
    {
        return outcome == ViewportProviderSessionOpenTransportOutcome::Opened;
    }
    [[nodiscard]] bool isDeferred() const
    {
        return outcome == ViewportProviderSessionOpenTransportOutcome::Deferred;
    }
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
    ~ViewportProviderBridge();
    ViewportProviderBridge(const ViewportProviderBridge&) = delete;
    ViewportProviderBridge& operator=(const ViewportProviderBridge&) = delete;
    ViewportProviderBridge(ViewportProviderBridge&&) = delete;
    ViewportProviderBridge& operator=(ViewportProviderBridge&&) = delete;

    ViewportProviderTransportResult closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    ViewportProviderTransportResult closeSession(quint64 generation, quint64 sessionSerial,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    ViewportProviderTransportResult activateSession(quint64 generation, quint64 sessionSerial);
    [[nodiscard]] bool canAdmitSession();
    ViewportProviderSessionOpenTransportResult openSession(
        const ViewportProviderSessionOpenInput& input);
    ViewportProviderTransportResult deliverRequest(const ImageSequenceProviderRequest& request);
    void completeFrameEventDelivery(quint64 leaseId);
    void completeFailureEventDelivery(quint64 leaseId);
    void completeProviderEventDelivery(quint64 deliveryId);
    void reconcileLeases(const QSet<quint64>& liveLeaseIds);
    ViewportProviderCleanupResult drainCleanup(bool retryPendingSessions = true);
    [[nodiscard]] bool hasPendingCleanup() const;
    ViewportProviderCleanupResult releaseAllProviderLeases();
    void setExecutor(ViewportProviderExecutor& executor);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest();
    void failNextSessionCloseDeliveriesForTest(qsizetype count);
    void useSynchronousEventDeliveryForTest();
    [[nodiscard]] qsizetype retainedEventEndpointCountForTest() const;
#endif

private:
    bool takeForcedDeliveryFailureForTest();
    [[nodiscard]] ViewportProviderExecutor& executor() const;
    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    bool pruneDestroyedSessions();
    void pruneExpiredEventEndpoints();
    ViewportProviderCleanupResult releaseLease(quint64 leaseId);
    void retrySessionCleanup(ViewportProviderCleanupResult& result, bool retryPendingSessions);

    enum class SessionLifecycle {
        Active,
        CleanupPending,
        Closing,
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
        std::shared_ptr<ViewportProviderSessionControl> control;
        std::shared_ptr<ViewportProviderEventEndpoint> eventEndpoint;
        QPointer<QObject> callbackTarget;
    };

    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<ImageSequenceProviderSession> activeSession;
    QHash<ImageSequenceProviderSession*, SessionRecord> sessions;
    std::shared_ptr<ViewportProviderLeaseRegistry> leaseRegistry;
    std::shared_ptr<ViewportProviderSessionCleanupRegistry> sessionCleanupRegistry;
    QVector<std::weak_ptr<ViewportProviderEventEndpoint>> eventEndpoints;
    bool forceNextCommandDeliveryFailure = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousEventDelivery = false;
#endif
};

[[nodiscard]] bool completeViewportProviderCleanupForApplicationShutdown();

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
