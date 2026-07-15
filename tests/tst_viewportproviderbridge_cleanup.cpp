#include "imageviewportdiagnostics_p.h"
#include "viewportproviderbridge_p.h"

#include <QtCore/QPointer>
#include <QtGui/QImage>
#include <QtTest/QTest>

#include <memory>

namespace {
class CleanupSession final
    : public ImageSequenceProviderSession // clazy:exclude=missing-qobject-macro
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

    ViewportProviderExecutorOutcome queueSessionClose(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken, ImageSequenceProviderRequestToken) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken, ImageSequenceProviderRequestToken) override
    {
        delete session;
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionDestruction(
        ImageSequenceProviderSession* session) override
    {
        ++destructionAttempts;
        if (destructionFailuresRemaining > 0) {
            --destructionFailuresRemaining;
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        delete session;
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(ImageSequenceProviderSession*,
        ImageSequenceProviderThreadingContract,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        ++releaseAttempts;
        if (releaseFailuresRemaining > 0) {
            --releaseFailuresRemaining;
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        delete frameHandle;
        return ViewportProviderExecutorOutcome::Completed;
    }

    int releaseFailuresRemaining = 0;
    int destructionFailuresRemaining = 0;
    int releaseAttempts = 0;
    int destructionAttempts = 0;
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
        Q_ASSERT(opened.opened);
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
        const auto closed = bridge.closeSession({}, {});
        Q_ASSERT(closed.delivered);
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

private slots:
    void releaseFailureRetainsLeaseUntilRetrySucceeds();
    void destructionFailureRetainsClosingSessionUntilRetrySucceeds();
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
    QCOMPARE(fixture.executor.destructionAttempts, 1);
    QVERIFY(!fixture.bridge.hasPendingCleanup());
}

void ViewportProviderBridgeCleanupTest::destructionFailureRetainsClosingSessionUntilRetrySucceeds()
{
    BridgeFixture fixture;
    const auto releaseCount = std::make_shared<int>(0);
    fixture.offerFrame(releaseCount);
    fixture.executor.destructionFailuresRemaining = 1;

    const ViewportProviderCleanupResult failed = fixture.bridge.drainCleanup();

    QCOMPARE(*releaseCount, 1);
    QVERIFY(!fixture.handle);
    QVERIFY(fixture.session);
    QCOMPARE(fixture.executor.destructionAttempts, 1);
    QVERIFY(fixture.bridge.hasPendingCleanup());
    QCOMPARE(failed.diagnostics.size(), 1);
    const auto destructionDiagnostic = failed.diagnostics.constFirst();
    QCOMPARE(destructionDiagnostic.operation,
        ImageViewportInternal::ProviderTransportOperation::Destruction);
    QCOMPARE(destructionDiagnostic.generation, quint64(17));
    QCOMPARE(destructionDiagnostic.sessionSerial, quint64(23));
    QCOMPARE(destructionDiagnostic.providerLeaseId, quint64(0));
    ImageViewportInternal::InternalObservability observability;
    observability.recordProviderCleanupFailure(destructionDiagnostic);
    const auto destructionObservation = observability.observations().constLast();
    QCOMPARE(destructionObservation.cause,
        ImageViewportInternal::InternalObservationCause::ProviderDestructionFailed);
    QCOMPARE(destructionObservation.identity.generation, quint64(17));
    QCOMPARE(destructionObservation.identity.sessionSerial, quint64(23));

    fixture.bridge.drainCleanup();

    QCOMPARE(*releaseCount, 1);
    QVERIFY(!fixture.session);
    QCOMPARE(fixture.executor.destructionAttempts, 2);
    QVERIFY(!fixture.bridge.hasPendingCleanup());
}

QTEST_MAIN(ViewportProviderBridgeCleanupTest)

#include "tst_viewportproviderbridge_cleanup.moc"
