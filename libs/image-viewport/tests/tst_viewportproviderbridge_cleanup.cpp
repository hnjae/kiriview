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
        emit providerEvent(ImageSequenceProviderEvent::frameReady(
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
        sessionControl->completeFrameReleaseOnSessionAffinity();
        return ViewportProviderExecutorOutcome::Completed;
    }

    int releaseFailuresRemaining = 0;
    int releaseAttempts = 0;
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
        emit session->providerEvent(ImageSequenceProviderEvent::frameReady(
            {}, handle, ImageSequenceProviderFrameEnvelope::stillFrame()));
        Q_ASSERT(event.frameLeaseId != 0);
        bridge.completeFrameEventDelivery(event.frameLeaseId);
        bridge.reconcileFrameLeases({});
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

private slots:
    void releaseFailureRetainsLeaseUntilRetrySucceeds();
    void pendingSessionCleanupSurvivesBridgeDestruction();
    void scheduledCloseIsNotDuplicatedAfterBridgeDestruction();
    void completedSessionsDoNotAccumulateEventEndpoints();
    void queuedEndpointRemainsRevocableAfterSessionCleanup();
    void activeAdvisoryBurstIsCoalescedAheadOfTerminal();
    void staleAndMismatchAdvisoryBurstsPreserveRepresentativeIdentity();
    void advisoryBurstDoesNotLoseFrameOwnership();
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

    emit session->providerEvent(ImageSequenceProviderEvent::waiting({}));
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
        emit session->providerEvent(index % 2 == 0
                ? ImageSequenceProviderEvent::waiting(token)
                : ImageSequenceProviderEvent::progress(token, 0.5));
    }
    emit session->providerEvent(ImageSequenceProviderEvent::failed(
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
        emit session->providerEvent(ImageSequenceProviderEvent::progress(retiredToken, 0.5));
        emit session->providerEvent(ImageSequenceProviderEvent::waiting(mismatchToken));
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
        emit session->providerEvent(ImageSequenceProviderEvent::progress(token, 0.5));
    }
    const auto releaseCount = std::make_shared<int>(0);
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    auto* handle = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emit session->providerEvent(ImageSequenceProviderEvent::frameReady(
        token, handle, ImageSequenceProviderFrameEnvelope::stillFrame()));

    QCoreApplication::sendPostedEvents(&callbackTarget, QEvent::MetaCall);

    QCOMPARE(deliveredEvents.size(), 2);
    QCOMPARE(deliveredEvents.at(1).kind, ImageSequenceProviderEventKind::FrameReady);
    QVERIFY(deliveredEvents.at(1).frameLeaseId != 0);
    bridge.completeFrameEventDelivery(deliveredEvents.at(1).frameLeaseId);
    bridge.reconcileFrameLeases({});
    QVERIFY(bridge.closeSession({}, {}).delivered);
    bridge.drainCleanup();
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!session);
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
        bridge.releaseAllFrameLeases();
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
                emit session->providerEvent(ImageSequenceProviderEvent::frameReady(
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
        bridge.reconcileFrameLeases({});
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
