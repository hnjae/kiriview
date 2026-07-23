// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportdiagnostics_p.h"
#include "imageviewporttoken_p.h"
#include "viewportproviderbridge_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtTest/QTest>

#include <atomic>
#include <memory>
#include <thread>

namespace {
class CleanupSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Close) {
            ++closeCount;
        }
    }

    int closeCount = 0;
};

class TrackedCleanupSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    TrackedCleanupSession(std::atomic<int>& closeCount, std::atomic<QThread*>& destructionThread,
        QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_closeCount(closeCount)
        , m_destructionThread(destructionThread)
    {
    }

    TrackedCleanupSession(const TrackedCleanupSession&) = delete;
    TrackedCleanupSession& operator=(const TrackedCleanupSession&) = delete;
    TrackedCleanupSession(TrackedCleanupSession&&) = delete;
    TrackedCleanupSession& operator=(TrackedCleanupSession&&) = delete;

    ~TrackedCleanupSession() override { m_destructionThread.store(QThread::currentThread()); }

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Close) {
            m_closeCount.fetch_add(1);
        }
    }

private:
    std::atomic<int>& m_closeCount;
    std::atomic<QThread*>& m_destructionThread;
};

class IngressSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    IngressSession(std::atomic<int>& nextOrder, std::atomic<int>& destructionOrder,
        std::atomic<QThread*>& destructionThread, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_nextOrder(nextOrder)
        , m_destructionOrder(destructionOrder)
        , m_destructionThread(destructionThread)
    {
    }

    IngressSession(const IngressSession&) = delete;
    IngressSession& operator=(const IngressSession&) = delete;
    IngressSession(IngressSession&&) = delete;
    IngressSession& operator=(IngressSession&&) = delete;

    ~IngressSession() override
    {
        m_destructionThread.store(QThread::currentThread());
        m_destructionOrder.store(m_nextOrder.fetch_add(1));
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() != ImageSequenceProviderRequestKind::Close || !m_closeFrameHandle) {
            return;
        }
        Q_EMIT providerEvent(ImageSequenceProviderEvent::frameReady(
            {}, m_closeFrameHandle, ImageSequenceProviderFrameEnvelope::stillFrame()));
        m_closeFrameHandle = nullptr;
    }

    void setCloseFrameHandle(ImageSequenceProviderFrameHandle* frameHandle)
    {
        m_closeFrameHandle = frameHandle;
    }

private:
    std::atomic<int>& m_nextOrder;
    std::atomic<int>& m_destructionOrder;
    std::atomic<QThread*>& m_destructionThread;
    ImageSequenceProviderFrameHandle* m_closeFrameHandle = nullptr;
};

class SynchronousAffinitySession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    SynchronousAffinitySession(std::atomic<QThread*>& metadataRequestThread,
        std::atomic<QThread*>& frameRequestThread, std::atomic<int>& frameReleaseCount,
        QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestThread(metadataRequestThread)
        , m_frameRequestThread(frameRequestThread)
        , m_frameReleaseCount(frameReleaseCount)
    {
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            m_metadataRequestThread.store(QThread::currentThread());
            Q_EMIT providerEvent(ImageSequenceProviderEvent::metadataReady(
                request.token(), ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0))));
            return;
        }
        if (request.kind() != ImageSequenceProviderRequestKind::Frame) {
            return;
        }

        m_frameRequestThread.store(QThread::currentThread());
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto* handle = new ImageSequenceProviderFrameHandle(
            new ImageFrame(image), [this](ImageFrame* frame) {
                m_frameReleaseCount.fetch_add(1);
                delete frame;
            });
        Q_EMIT providerEvent(ImageSequenceProviderEvent::frameReady(
            request.token(), handle, ImageSequenceProviderFrameEnvelope::stillFrame()));
    }

private:
    std::atomic<QThread*>& m_metadataRequestThread;
    std::atomic<QThread*>& m_frameRequestThread;
    std::atomic<int>& m_frameReleaseCount;
};

class FaultInjectingExecutor final : public ViewportProviderExecutor
{
public:
    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract, std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        command();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken, ImageSequenceProviderRequestToken) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        session->request(ImageSequenceProviderRequest::close());
        sessionControl->completeCloseOnSessionAffinity();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        ++releaseAttempts;
        if (releaseFailuresRemaining > 0) {
            --releaseFailuresRemaining;
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        delete frameHandle;
        sessionControl->completeHandleReleaseOnSessionAffinity();
        return ViewportProviderExecutorOutcome::Completed;
    }

    int releaseFailuresRemaining = 0;
    int releaseAttempts = 0;
};

class StallingCloseExecutor final : public ViewportProviderExecutor
{
public:
    StallingCloseExecutor() = default;
    ~StallingCloseExecutor() override { completeAllCloses(); }
    StallingCloseExecutor(const StallingCloseExecutor&) = delete;
    StallingCloseExecutor& operator=(const StallingCloseExecutor&) = delete;
    StallingCloseExecutor(StallingCloseExecutor&&) = delete;
    StallingCloseExecutor& operator=(StallingCloseExecutor&&) = delete;

    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract, std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        command();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken, ImageSequenceProviderRequestToken) override
    {
        if (!sessionControl || !sessionControl->session()) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        pendingCloses.append(sessionControl);
        return ViewportProviderExecutorOutcome::Scheduled;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>&,
        ImageSequenceProviderFrameHandle*) override
    {
        return ViewportProviderExecutorOutcome::RetryableFailure;
    }

    void completeNextClose()
    {
        if (pendingCloses.isEmpty()) {
            return;
        }
        const auto sessionControl = pendingCloses.takeFirst();
        ImageSequenceProviderSession* session = sessionControl->session();
        if (!session) {
            return;
        }
        session->request(ImageSequenceProviderRequest::close());
        sessionControl->completeCloseOnSessionAffinity();
    }

    void completeAllCloses()
    {
        while (!pendingCloses.isEmpty()) {
            completeNextClose();
        }
    }

private:
    QVector<std::shared_ptr<ViewportProviderSessionControl>> pendingCloses;
};

struct BridgeFixture
{
    BridgeFixture()
    {
        bridge.setExecutor(executor);
        bridge.useSynchronousEventDeliveryForTest();
        factory = std::make_shared<ImageSequenceProviderSessionFactory>([this]() {
            session = new CleanupSession;
            return ImageSequenceProviderSessionFactoryResult::created(session);
        });
        const auto opened = bridge.openSession(
            { factory, ImageSequenceProviderThreadingContract::AffinityBound, 17, 23,
                &callbackTarget, [this](const ViewportProviderEvent& value) { event = value; } });
        if (!opened.opened) {
            qFatal("provider cleanup fixture could not open its session");
        }
    }

    void offerFrame(const std::shared_ptr<int>& releaseCount)
    {
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        handle = new ImageSequenceProviderFrameHandle(
            new ImageFrame(image), [releaseCount](ImageFrame* frame) {
                ++*releaseCount;
                delete frame;
            });
        Q_EMIT session->providerEvent(ImageSequenceProviderEvent::frameReady(
            {}, handle, ImageSequenceProviderFrameEnvelope::stillFrame()));
        Q_ASSERT(event.frameLeaseId != 0);
        bridge.completeFrameEventDelivery(event.frameLeaseId);
        bridge.reconcileLeases({});
        if (!bridge.closeSession({}, {}).delivered) {
            qFatal("test provider session close failed");
        }
    }

    QObject callbackTarget;
    FaultInjectingExecutor executor;
    ViewportProviderBridge bridge;
    std::shared_ptr<ImageSequenceProviderSessionFactory> factory;
    QPointer<CleanupSession> session;
    QPointer<ImageSequenceProviderFrameHandle> handle;
    ViewportProviderEvent event;
};
}

class ViewportProviderBridgeCleanupTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportProviderBridgeCleanupTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void releaseFailureRetainsLeaseUntilRetrySucceeds();
    void pendingSessionCleanupSurvivesBridgeDestruction();
    void scheduledCloseIsNotDuplicatedAfterBridgeDestruction();
    void completedSessionsDoNotAccumulateEventEndpoints();
    void stalledClosesBoundSessionAdmission();
    void queuedEndpointRemainsRevocableAfterSessionCleanup();
    void activeAdvisoryBurstIsCoalescedAheadOfTerminal();
    void staleAndMismatchAdvisoryBurstsPreserveRepresentativeIdentity();
    void advisoryBurstDoesNotLoseFrameOwnership();
    void nonAdvisoryBurstIsBackpressuredBeforeLeaseClaim();
    void affinityBoundSynchronousResponseCompletesDuringActiveDelivery();
    void queuedFrameIngressOwnsHandleBeforeSessionClose_data();
    void queuedFrameIngressOwnsHandleBeforeSessionClose();
};

void ViewportProviderBridgeCleanupTest::releaseFailureRetainsLeaseUntilRetrySucceeds()
{
    BridgeFixture fixture;
    const auto releaseCount = std::make_shared<int>(0);
    fixture.offerFrame(releaseCount);
    fixture.executor.releaseFailuresRemaining = 1;

    const ViewportProviderCleanupResult failed = fixture.bridge.drainCleanup();

    QCOMPARE(fixture.executor.releaseAttempts, 1);
    QCOMPARE(*releaseCount, 0);
    QVERIFY(fixture.handle);
    QVERIFY(fixture.session);
    QVERIFY(fixture.bridge.hasPendingCleanup());
    QCOMPARE(failed.diagnostics.size(), 1);
    const auto releaseDiagnostic = failed.diagnostics.constFirst();
    QCOMPARE(
        releaseDiagnostic.operation, ImageViewportInternal::ProviderTransportOperation::Release);
    QCOMPARE(releaseDiagnostic.role, ImageViewportPageRole::Primary);
    QCOMPARE(releaseDiagnostic.generation, quint64(17));
    QCOMPARE(releaseDiagnostic.sessionSerial, quint64(23));
    QCOMPARE(releaseDiagnostic.providerLeaseId, fixture.event.frameLeaseId);
    ImageViewportInternal::InternalObservability observability;
    observability.recordProviderCleanupFailure(releaseDiagnostic);
    const auto releaseObservation = observability.observations().constLast();
    QCOMPARE(releaseObservation.cause,
        ImageViewportInternal::InternalObservationCause::ProviderReleaseFailed);
    QCOMPARE(releaseObservation.identity.generation, quint64(17));
    QCOMPARE(releaseObservation.identity.sessionSerial, quint64(23));
    QCOMPARE(releaseObservation.identity.providerLeaseId, fixture.event.frameLeaseId);

    fixture.bridge.drainCleanup();

    QCOMPARE(fixture.executor.releaseAttempts, 2);
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!fixture.handle);
    QVERIFY(!fixture.session);
    QVERIFY(!fixture.bridge.hasPendingCleanup());
}

void ViewportProviderBridgeCleanupTest::pendingSessionCleanupSurvivesBridgeDestruction()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    std::atomic<int> closeCount { 0 };
    std::atomic<QThread*> destructionThread { nullptr };
    QPointer<TrackedCleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&]() {
        auto* created = new TrackedCleanupSession(closeCount, destructionThread);
        created->moveToThread(&workerThread);
        session = created;
        return ImageSequenceProviderSessionFactoryResult::created(created);
    });
    QObject callbackTarget;
    auto bridge = std::make_unique<ViewportProviderBridge>();
    const auto opened
        = bridge->openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 17,
            23, &callbackTarget, [](const ViewportProviderEvent&) { } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    bridge->failNextSessionCloseDeliveriesForTest(3);
    QVERIFY(!bridge->closeSession({}, {}).delivered);
    QVERIFY(bridge->drainCleanup().pending);

    bridge.reset();

    QVERIFY(session);
    QTRY_VERIFY_WITH_TIMEOUT(!session, 3000);
    QCOMPARE(closeCount.load(), 1);
    QCOMPARE(destructionThread.load(), &workerThread);
}

void ViewportProviderBridgeCleanupTest::scheduledCloseIsNotDuplicatedAfterBridgeDestruction()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });
    QSemaphore workerBlocked;
    QSemaphore releaseWorker;
    QObject workerMarker;
    workerMarker.moveToThread(&workerThread);
    QVERIFY(QMetaObject::invokeMethod(
        &workerMarker,
        [&]() {
            workerBlocked.release();
            releaseWorker.acquire();
        },
        Qt::QueuedConnection));
    workerBlocked.acquire();
    const auto unblockWorker = qScopeGuard([&releaseWorker]() { releaseWorker.release(); });

    std::atomic<int> closeCount { 0 };
    std::atomic<QThread*> destructionThread { nullptr };
    QPointer<TrackedCleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&]() {
        auto* created = new TrackedCleanupSession(closeCount, destructionThread);
        created->moveToThread(&workerThread);
        session = created;
        return ImageSequenceProviderSessionFactoryResult::created(created);
    });
    QObject callbackTarget;
    auto bridge = std::make_unique<ViewportProviderBridge>();
    const auto opened
        = bridge->openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 31,
            47, &callbackTarget, [](const ViewportProviderEvent&) { } });
    QVERIFY(opened.opened);
    QVERIFY(session);
    QVERIFY(bridge->closeSession({}, {}).delivered);

    bridge.reset();

    QVERIFY(session);
    releaseWorker.release();
    QTRY_VERIFY_WITH_TIMEOUT(!session, 3000);
    QCOMPARE(closeCount.load(), 1);
    QCOMPARE(destructionThread.load(), &workerThread);
}

void ViewportProviderBridgeCleanupTest::completedSessionsDoNotAccumulateEventEndpoints()
{
    QObject callbackTarget;
    ViewportProviderBridge bridge;
    bridge.setExecutor(synchronousViewportProviderExecutorForTest());

    for (quint64 generation = 1; generation <= 3; ++generation) {
        QPointer<CleanupSession> session;
        auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
            session = new CleanupSession;
            return ImageSequenceProviderSessionFactoryResult::created(session);
        });
        const auto opened
            = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound,
                generation, generation, &callbackTarget, [](const ViewportProviderEvent&) { } });
        QVERIFY(opened.opened);
        QVERIFY(session);
        QCOMPARE(bridge.retainedEventEndpointCountForTest(), qsizetype(1));

        const auto closed = bridge.closeSession({}, {});
        QVERIFY(closed.delivered);
        QVERIFY(!session);
        bridge.drainCleanup();

        QVERIFY(!bridge.hasPendingCleanup());
        QCOMPARE(bridge.retainedEventEndpointCountForTest(), qsizetype(0));
    }
}

void ViewportProviderBridgeCleanupTest::stalledClosesBoundSessionAdmission()
{
    QObject callbackTarget;
    StallingCloseExecutor executor;
    ViewportProviderBridge bridge;
    bridge.setExecutor(executor);
    int factoryInvocationCount = 0;
    QVector<QPointer<CleanupSession>> sessions;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&]() {
        ++factoryInvocationCount;
        auto* session = new CleanupSession;
        sessions.append(session);
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto open = [&](quint64 identity) {
        return bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound,
            identity, identity, &callbackTarget, [](const ViewportProviderEvent&) { } });
    };

    QVERIFY(open(1).opened);
    QVERIFY(bridge.closeSession({}, {}).delivered);
    QVERIFY(open(2).opened);
    QVERIFY(bridge.closeSession({}, {}).delivered);
    QCOMPARE(factoryInvocationCount, 2);
    QCOMPARE(bridge.retainedEventEndpointCountForTest(), qsizetype(2));

    const auto rejected = open(3);

    QVERIFY(!rejected.opened);
    QVERIFY(!rejected.providerFailureAvailable);
    QCOMPARE(rejected.providerFailureLeaseId, quint64(0));
    QCOMPARE(factoryInvocationCount, 2);
    QCOMPARE(bridge.retainedEventEndpointCountForTest(), qsizetype(2));

    executor.completeNextClose();
    QVERIFY(!sessions[0]);
    QVERIFY(open(4).opened);
    QCOMPARE(factoryInvocationCount, 3);
    QCOMPARE(bridge.retainedEventEndpointCountForTest(), qsizetype(2));
    QVERIFY(bridge.closeSession({}, {}).delivered);
}

void ViewportProviderBridgeCleanupTest::queuedEndpointRemainsRevocableAfterSessionCleanup()
{
    QObject callbackTarget;
    int deliveryCount = 0;
    auto bridge = std::make_unique<ViewportProviderBridge>();
    bridge->setExecutor(synchronousViewportProviderExecutorForTest());
    QPointer<CleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
        session = new CleanupSession;
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto opened = bridge->openSession(
        { factory, ImageSequenceProviderThreadingContract::AffinityBound, 17, 23, &callbackTarget,
            [&deliveryCount](const ViewportProviderEvent&) { ++deliveryCount; } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::waiting({}));
    const auto closed = bridge->closeSession({}, {});
    QVERIFY(closed.delivered);
    QVERIFY(!session);
    bridge->drainCleanup();
    QCOMPARE(bridge->retainedEventEndpointCountForTest(), qsizetype(1));

    bridge.reset();
    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QCOMPARE(deliveryCount, 0);
}

void ViewportProviderBridgeCleanupTest::activeAdvisoryBurstIsCoalescedAheadOfTerminal()
{
    QObject callbackTarget;
    QVector<ViewportProviderEvent> deliveredEvents;
    ViewportProviderBridge bridge;
    bridge.setExecutor(synchronousViewportProviderExecutorForTest());
    QPointer<CleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
        session = new CleanupSession;
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto opened
        = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 17,
            23, &callbackTarget, [&deliveredEvents](const ViewportProviderEvent& event) {
                deliveredEvents.append(event);
            } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    const auto token = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(1);
    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(token)).delivered);
    for (int index = 0; index < 1024; ++index) {
        Q_EMIT session->providerEvent(index % 2 == 0
                ? ImageSequenceProviderEvent::waiting(token)
                : ImageSequenceProviderEvent::progress(token, 0.5));
    }
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::failed(
        token, ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal)));
    QCOMPARE(deliveredEvents.size(), 0);

    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QCOMPARE(deliveredEvents.size(), 2);
    QVERIFY(deliveredEvents.at(0).kind == ImageSequenceProviderEventKind::Waiting
        || deliveredEvents.at(0).kind == ImageSequenceProviderEventKind::Progress);
    QCOMPARE(deliveredEvents.at(0).token, token);
    QCOMPARE(deliveredEvents.at(1).kind, ImageSequenceProviderEventKind::Failed);
    QCOMPARE(deliveredEvents.at(1).token, token);

    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
}

void ViewportProviderBridgeCleanupTest::
    staleAndMismatchAdvisoryBurstsPreserveRepresentativeIdentity()
{
    QObject callbackTarget;
    QVector<ViewportProviderEvent> deliveredEvents;
    ViewportProviderBridge bridge;
    bridge.setExecutor(synchronousViewportProviderExecutorForTest());
    QPointer<CleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
        session = new CleanupSession;
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto opened
        = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 17,
            23, &callbackTarget, [&deliveredEvents](const ViewportProviderEvent& event) {
                deliveredEvents.append(event);
            } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    const auto retiredToken
        = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(1);
    const auto activeToken = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(2);
    const auto mismatchToken
        = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(3);
    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(retiredToken)).delivered);
    QVERIFY(
        bridge.deliverRequest(ImageSequenceProviderRequest::cancel({ retiredToken })).delivered);
    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(activeToken)).delivered);

    for (int index = 0; index < 1024; ++index) {
        Q_EMIT session->providerEvent(ImageSequenceProviderEvent::progress(retiredToken, 0.5));
        Q_EMIT session->providerEvent(ImageSequenceProviderEvent::waiting(mismatchToken));
    }
    QCOMPARE(deliveredEvents.size(), 0);

    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QCOMPARE(deliveredEvents.size(), 2);
    QCOMPARE(deliveredEvents.at(0).token, retiredToken);
    QCOMPARE(deliveredEvents.at(1).token, mismatchToken);

    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
}

void ViewportProviderBridgeCleanupTest::advisoryBurstDoesNotLoseFrameOwnership()
{
    QObject callbackTarget;
    QVector<ViewportProviderEvent> deliveredEvents;
    ViewportProviderBridge bridge;
    bridge.setExecutor(synchronousViewportProviderExecutorForTest());
    QPointer<CleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
        session = new CleanupSession;
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto opened
        = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 17,
            23, &callbackTarget, [&deliveredEvents](const ViewportProviderEvent& event) {
                deliveredEvents.append(event);
            } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    const auto token = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(1);
    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(token)).delivered);
    for (int index = 0; index < 1024; ++index) {
        Q_EMIT session->providerEvent(ImageSequenceProviderEvent::progress(token, 0.5));
    }
    const auto releaseCount = std::make_shared<int>(0);
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    auto* handle = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::frameReady(
        token, handle, ImageSequenceProviderFrameEnvelope::stillFrame()));

    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QCOMPARE(deliveredEvents.size(), 2);
    QCOMPARE(deliveredEvents.at(1).kind, ImageSequenceProviderEventKind::FrameReady);
    QVERIFY(deliveredEvents.at(1).frameLeaseId != 0);
    bridge.completeFrameEventDelivery(deliveredEvents.at(1).frameLeaseId);
    bridge.reconcileLeases({});
    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!session);
}

void ViewportProviderBridgeCleanupTest::nonAdvisoryBurstIsBackpressuredBeforeLeaseClaim()
{
    QObject callbackTarget;
    QVector<ViewportProviderEvent> deliveredEvents;
    ViewportProviderBridge bridge;
    bridge.setExecutor(synchronousViewportProviderExecutorForTest());
    QPointer<CleanupSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&session]() {
        session = new CleanupSession;
        return ImageSequenceProviderSessionFactoryResult::created(session);
    });
    const auto opened
        = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::ThreadSafe, 17, 23,
            &callbackTarget, [&](const ViewportProviderEvent& event) {
                deliveredEvents.append(event);
                bridge.completeFrameEventDelivery(event.frameLeaseId);
                bridge.completeFailureEventDelivery(event.failureLeaseId);
                bridge.reconcileLeases({});
                bridge.drainCleanup();
            } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    const auto activeToken = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(1);
    const auto mismatchToken
        = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(2);
    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(activeToken)).delivered);

    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });
    QObject workerMarker;
    workerMarker.moveToThread(&workerThread);
    QSemaphore emissionEntered;
    QSemaphore emissionReturned;
    QSemaphore allEmissionsReturned;
    std::atomic<int> returnedCount { 0 };
    std::atomic<int> frameReleaseCount { 0 };
    std::atomic<int> failureReleaseCount { 0 };
    QVERIFY(QMetaObject::invokeMethod(
        &workerMarker,
        [&]() {
            auto emitFailure = [&](ImageSequenceProviderRequestToken token) {
                emissionEntered.release();
                auto* handle = new ImageSequenceProviderFailureHandle(
                    [&failureReleaseCount] { failureReleaseCount.fetch_add(1); });
                Q_EMIT session->providerEvent(ImageSequenceProviderEvent::failed(token,
                    ImageSequenceProviderFailure(
                        ImageSequenceProviderFailureCause::ProviderInternal, handle)));
                returnedCount.fetch_add(1);
                emissionReturned.release();
            };
            emitFailure(activeToken);

            emissionEntered.release();
            QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            auto* frameHandle = new ImageSequenceProviderFrameHandle(
                new ImageFrame(image), [&frameReleaseCount](ImageFrame* frame) {
                    frameReleaseCount.fetch_add(1);
                    delete frame;
                });
            Q_EMIT session->providerEvent(ImageSequenceProviderEvent::frameReady(
                activeToken, frameHandle, ImageSequenceProviderFrameEnvelope::stillFrame()));
            returnedCount.fetch_add(1);
            emissionReturned.release();

            emitFailure(mismatchToken);
            allEmissionsReturned.release();
        },
        Qt::QueuedConnection));

    emissionEntered.acquire();
    emissionReturned.acquire();
    emissionEntered.acquire();
    const bool burstReturnedBeforeDelivery = allEmissionsReturned.tryAcquire(1, 100);

    while (returnedCount.load() < 3) {
        QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);
        QVERIFY(emissionReturned.tryAcquire(1, 1000));
    }
    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);
    QVERIFY(QMetaObject::invokeMethod(&workerMarker, []() { }, Qt::BlockingQueuedConnection));

    QVERIFY(!burstReturnedBeforeDelivery);
    QCOMPARE(deliveredEvents.size(), 3);
    QCOMPARE(deliveredEvents.at(0).kind, ImageSequenceProviderEventKind::Failed);
    QCOMPARE(deliveredEvents.at(0).token, activeToken);
    QCOMPARE(deliveredEvents.at(1).kind, ImageSequenceProviderEventKind::FrameReady);
    QCOMPARE(deliveredEvents.at(1).token, activeToken);
    QCOMPARE(deliveredEvents.at(2).kind, ImageSequenceProviderEventKind::Failed);
    QCOMPARE(deliveredEvents.at(2).token, mismatchToken);
    QCOMPARE(frameReleaseCount.load(), 1);
    QCOMPARE(failureReleaseCount.load(), 2);

    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
    QVERIFY(!session);
}

void ViewportProviderBridgeCleanupTest::
    affinityBoundSynchronousResponseCompletesDuringActiveDelivery()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    std::atomic<QThread*> metadataRequestThread { nullptr };
    std::atomic<QThread*> frameRequestThread { nullptr };
    std::atomic<int> frameReleaseCount { 0 };
    QPointer<SynchronousAffinitySession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&]() {
        auto* created = new SynchronousAffinitySession(
            metadataRequestThread, frameRequestThread, frameReleaseCount);
        created->moveToThread(&workerThread);
        session = created;
        return ImageSequenceProviderSessionFactoryResult::created(created);
    });

    const auto metadataToken
        = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(1);
    const auto frameToken = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(2);
    ImageSequenceProviderDisplayDemand demand;
    demand.setRole(ImageViewportPageRole::Primary);
    demand.setResolvedFrame(0);
    const ImageSequenceProviderRequest frameRequest = ImageSequenceProviderRequest::frame(
        frameToken, ImageViewportPageRole::Primary, 0, demand);

    QObject callbackTarget;
    ViewportProviderBridge bridge;
    QVector<ImageSequenceProviderEventKind> deliveredKinds;
    quint64 frameLeaseId = 0;
    QSemaphore frameRequestCompleted;
    bool frameRequestCompletedDuringMetadataDelivery = false;
    std::atomic<bool> frameRequestDelivered { false };
    std::unique_ptr<std::thread> frameRequester;
    const auto opened
        = bridge.openSession({ factory, ImageSequenceProviderThreadingContract::AffinityBound, 17,
            23, &callbackTarget, [&](const ViewportProviderEvent& event) {
                deliveredKinds.append(event.kind);
                if (event.kind == ImageSequenceProviderEventKind::MetadataReady) {
                    frameRequester = std::make_unique<std::thread>([&]() {
                        frameRequestDelivered.store(bridge.deliverRequest(frameRequest).delivered);
                        frameRequestCompleted.release();
                    });
                    frameRequestCompletedDuringMetadataDelivery
                        = frameRequestCompleted.tryAcquire(1, 1000);
                } else if (event.kind == ImageSequenceProviderEventKind::FrameReady) {
                    frameLeaseId = event.frameLeaseId;
                }
            } });
    QVERIFY(opened.opened);
    QVERIFY(session);

    QVERIFY(bridge.deliverRequest(ImageSequenceProviderRequest::metadata(metadataToken)).delivered);
    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);
    QVERIFY(frameRequester);
    frameRequester->join();
    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QVERIFY(frameRequestCompletedDuringMetadataDelivery);
    QVERIFY(frameRequestDelivered.load());
    QCOMPARE(metadataRequestThread.load(), &workerThread);
    QCOMPARE(frameRequestThread.load(), &workerThread);
    const QVector expectedKinds { ImageSequenceProviderEventKind::MetadataReady,
        ImageSequenceProviderEventKind::FrameReady };
    QCOMPARE(deliveredKinds, expectedKinds);
    QVERIFY(frameLeaseId != 0);

    bridge.completeFrameEventDelivery(frameLeaseId);
    bridge.reconcileLeases({});
    bridge.drainCleanup();
    QTRY_COMPARE(frameReleaseCount.load(), 1);

    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
    QTRY_VERIFY(!session);
}

void ViewportProviderBridgeCleanupTest::queuedFrameIngressOwnsHandleBeforeSessionClose_data()
{
    QTest::addColumn<ImageSequenceProviderThreadingContract>("threadingContract");
    QTest::addColumn<bool>("emitDuringClose");
    QTest::addColumn<bool>("shutdownRetired");

    QTest::newRow("affinity-bound-before-close")
        << ImageSequenceProviderThreadingContract::AffinityBound << false << false;
    QTest::newRow("thread-safe-before-close")
        << ImageSequenceProviderThreadingContract::ThreadSafe << false << false;
    QTest::newRow("affinity-bound-during-close")
        << ImageSequenceProviderThreadingContract::AffinityBound << true << false;
    QTest::newRow("thread-safe-during-close")
        << ImageSequenceProviderThreadingContract::ThreadSafe << true << false;
    QTest::newRow("affinity-bound-shutdown")
        << ImageSequenceProviderThreadingContract::AffinityBound << true << true;
    QTest::newRow("thread-safe-shutdown")
        << ImageSequenceProviderThreadingContract::ThreadSafe << true << true;
}

void ViewportProviderBridgeCleanupTest::queuedFrameIngressOwnsHandleBeforeSessionClose()
{
    QFETCH(ImageSequenceProviderThreadingContract, threadingContract);
    QFETCH(bool, emitDuringClose);
    QFETCH(bool, shutdownRetired);

    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });
    QObject workerMarker;
    workerMarker.moveToThread(&workerThread);

    std::atomic<int> nextOrder { 1 };
    std::atomic<int> releaseCount { 0 };
    std::atomic<int> releaseOrder { 0 };
    std::atomic<int> destructionOrder { 0 };
    std::atomic<QThread*> releaseThread { nullptr };
    std::atomic<QThread*> destructionThread { nullptr };
    QPointer<IngressSession> session;
    auto factory = std::make_shared<ImageSequenceProviderSessionFactory>([&]() {
        auto* created = new IngressSession(nextOrder, destructionOrder, destructionThread);
        created->moveToThread(&workerThread);
        session = created;
        return ImageSequenceProviderSessionFactoryResult::created(created);
    });

    QObject callbackTarget;
    ViewportProviderEvent deliveredEvent;
    ViewportProviderBridge bridge;
    const auto opened = bridge.openSession({ factory, threadingContract, 31, 47, &callbackTarget,
        [&](const ViewportProviderEvent& event) { deliveredEvent = event; } });
    QVERIFY(opened.opened);
    QVERIFY(session);
    if (shutdownRetired) {
        bridge.releaseAllProviderLeases();
    }

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    auto* handle
        = new ImageSequenceProviderFrameHandle(new ImageFrame(image), [&](ImageFrame* frame) {
              releaseThread.store(QThread::currentThread());
              releaseOrder.store(nextOrder.fetch_add(1));
              releaseCount.fetch_add(1);
              delete frame;
          });
    if (emitDuringClose) {
        QVERIFY(QMetaObject::invokeMethod(
            session, [session, handle]() { session->setCloseFrameHandle(handle); },
            Qt::BlockingQueuedConnection));
    } else {
        QVERIFY(QMetaObject::invokeMethod(
            session,
            [session, handle]() {
                Q_EMIT session->providerEvent(ImageSequenceProviderEvent::frameReady(
                    {}, handle, ImageSequenceProviderFrameEnvelope::stillFrame()));
            },
            Qt::BlockingQueuedConnection));
    }

    const auto closed = bridge.closeSession({}, {});
    QVERIFY(closed.delivered);
    QVERIFY(QMetaObject::invokeMethod(&workerMarker, []() { }, Qt::BlockingQueuedConnection));

    QVERIFY2(session, "session destruction overtook queued frame ownership capture");
    QCOMPARE(releaseCount.load(), 0);

    QCoreApplication::sendPostedEvents(
        shutdownRetired ? nullptr : &callbackTarget, QEvent::MetaCall);
    QVERIFY(deliveredEvent.frameLeaseId != 0);
    if (!shutdownRetired) {
        bridge.completeFrameEventDelivery(deliveredEvent.frameLeaseId);
        bridge.reconcileLeases({});
        bridge.drainCleanup();
    }
    QVERIFY(QMetaObject::invokeMethod(&workerMarker, []() { }, Qt::BlockingQueuedConnection));

    QCOMPARE(releaseCount.load(), 1);
    QVERIFY(!session);
    QCOMPARE(releaseThread.load(),
        threadingContract == ImageSequenceProviderThreadingContract::AffinityBound
            ? &workerThread
            : QThread::currentThread());
    QCOMPARE(destructionThread.load(), &workerThread);
    QVERIFY(releaseOrder.load() > 0);
    QVERIFY(destructionOrder.load() > releaseOrder.load());
}

QTEST_MAIN(ViewportProviderBridgeCleanupTest)

#include "tst_viewportproviderbridge_cleanup.moc"
