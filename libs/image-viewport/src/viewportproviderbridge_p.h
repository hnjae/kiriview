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
    void claimFrameLease();
    void endEventIngress();
    void completeCloseOnSessionAffinity();
    void completeFrameReleaseOnSessionAffinity();
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
    qsizetype frameLeaseCount = 0;
    bool closeCompleted = false;
    bool destructionStarted = false;
};

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
        ImageSequenceProviderFrameHandle* frameHandle)
        = 0;
    virtual ViewportProviderExecutorOutcome releaseFailureHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFailureHandle* failureHandle);
};

struct ViewportProviderTransportResult
{
    bool delivered = false;
    ImageViewportInternal::ProviderTransportDiagnostic diagnostic;
};

struct ViewportProviderSessionOpenTransportResult
{
    bool opened = false;
    bool providerFailureAvailable = false;
    ImageSequenceProviderFailureCause providerCause
        = ImageSequenceProviderFailureCause::Unavailable;
    ImageSequenceProviderFailureReference providerReference;
    quint64 providerFailureLeaseId = 0;
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
    ViewportProviderSessionOpenTransportResult openSession(
        const ViewportProviderSessionOpenInput& input);
    ViewportProviderTransportResult deliverRequest(const ImageSequenceProviderRequest& request);
    void completeFrameEventDelivery(quint64 leaseId);
    void completeFailureEventDelivery(quint64 leaseId);
    void reconcileFrameLeases(const QSet<quint64>& liveLeaseIds);
    void reconcileFailureLeases(const QSet<quint64>& liveLeaseIds);
    ViewportProviderCleanupResult drainCleanup(bool retryPendingSessions = true);
    bool hasPendingCleanup() const;
    ViewportProviderCleanupResult releaseAllFrameLeases();
    void setExecutor(ViewportProviderExecutor& executor);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest();
    void failNextSessionCloseDeliveriesForTest(qsizetype count);
    void useSynchronousEventDeliveryForTest();
    qsizetype retainedEventEndpointCountForTest() const;
#endif

private:
    bool takeForcedDeliveryFailureForTest();
    ViewportProviderExecutor& executor() const;
    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    void pruneExpiredEventEndpoints();
    ViewportProviderCleanupResult releaseFrameLease(quint64 leaseId);
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
    };

    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<ImageSequenceProviderSession> activeSession;
    QHash<ImageSequenceProviderSession*, SessionRecord> sessions;
    std::shared_ptr<ViewportProviderLeaseRegistry> frameLeaseRegistry;
    std::shared_ptr<ViewportProviderSessionCleanupRegistry> sessionCleanupRegistry;
    QVector<std::weak_ptr<ViewportProviderEventEndpoint>> eventEndpoints;
    bool forceNextCommandDeliveryFailure = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    bool synchronousEventDelivery = false;
#endif
};

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
