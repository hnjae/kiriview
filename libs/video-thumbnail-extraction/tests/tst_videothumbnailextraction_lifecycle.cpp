// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video_thumbnail_extraction_test_support.h"

#include <QImage>
#include <QObject>
#include <QTest>
#include <QThread>

#include <optional>

namespace {

using kiriview::VideoThumbnailExtractionFailureCause;
using kiriview::VideoThumbnailExtractionJob;
using kiriview::VideoThumbnailExtractionResult;
using kiriview::VideoThumbnailExtractionStatus;
using kiriview::detail::VideoThumbnailBackendError;
using kiriview::detail::VideoThumbnailEmbeddedImages;
using kiriview::test::ExtractionHarness;

class VideoThumbnailExtractionLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void completionIsQueuedAndJobDeactivatesBeforeCallback();
    void synchronousDeadlineExpiryStopsStartup();
    void cancellationBeforeStartupSuppressesDelivery();
    void cancellationSuppressesPendingDelivery();
    void activeCancellationReleasesResources();
    void physicalRetirementFollowsBackendDestruction();
    void receiverDestructionWhileActiveSuppressesDelivery();
    void receiverDestructionSuppressesPendingDelivery();
    void firstTerminalOutcomeWinsAcrossReentrantCleanup();
    void moveAssignmentCancelsReplacedOperation();
    void callbackMayReleaseJobAndReceiver();
};

void VideoThumbnailExtractionLifecycleTest::completionIsQueuedAndJobDeactivatesBeforeCallback()
{
    QObject receiver;
    VideoThumbnailExtractionJob* observedJob = nullptr;
    int completionCount = 0;
    bool inactiveInCallback = false;
    bool correctThread = false;

    auto job = kiriview::startVideoThumbnailExtraction(
        &receiver, {}, [&](VideoThumbnailExtractionResult result) {
            ++completionCount;
            inactiveInCallback = observedJob != nullptr && !observedJob->isActive();
            correctThread = QThread::currentThread() == receiver.thread();
            QCOMPARE(result.failure->cause, VideoThumbnailExtractionFailureCause::InvalidRequest);
        });
    observedJob = &job;

    QVERIFY(job.isActive());
    QCOMPARE(completionCount, 0);

    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 1);
    QVERIFY(inactiveInCallback);
    QVERIFY(correctThread);
    QVERIFY(!job.isActive());
}

void VideoThumbnailExtractionLifecycleTest::synchronousDeadlineExpiryStopsStartup()
{
    QObject receiver;
    ExtractionHarness harness;
    harness.deadline->expireSynchronouslyOnStart = true;
    int completionCount = 0;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&](VideoThumbnailExtractionResult value) {
            ++completionCount;
            result = std::move(value);
        },
        harness.dependencies());

    QVERIFY(job.isActive());
    QCOMPARE(completionCount, 0);
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 1);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, VideoThumbnailExtractionFailureCause::TimedOut);
    QVERIFY(!job.isActive());
    QCOMPARE(harness.deadline->startCalls, 1);
    QVERIFY(harness.deadline->stopCalls > 0);
    QCOMPARE(harness.backend->setSourceCalls, 0);
    QVERIFY(harness.backend->stopCalls > 0);
}

void VideoThumbnailExtractionLifecycleTest::cancellationBeforeStartupSuppressesDelivery()
{
    QObject receiver;
    ExtractionHarness harness;
    int completionCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, {}, [&completionCount](VideoThumbnailExtractionResult) { ++completionCount; },
        harness.dependencies());
    QVERIFY(job.isActive());

    job.cancel();
    job.cancel();
    QVERIFY(!job.isActive());
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 0);
}

void VideoThumbnailExtractionLifecycleTest::cancellationSuppressesPendingDelivery()
{
    QObject receiver;
    ExtractionHarness harness;
    int completionCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&completionCount](VideoThumbnailExtractionResult) { ++completionCount; },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    const auto staleBackendError = harness.backend->instance->errorCallback();
    const auto staleDeadlineExpiry = harness.deadline->expired;
    QVERIFY(staleBackendError);
    QVERIFY(staleDeadlineExpiry);
    QImage cover(8, 8, QImage::Format_RGBA8888);
    cover.fill(Qt::yellow);
    harness.backend->instance->emitMetadata(VideoThumbnailEmbeddedImages { cover, {} });

    QVERIFY(job.isActive());
    QCOMPARE(completionCount, 0);
    QVERIFY(harness.backend->stopCalls > 0);
    QVERIFY(harness.deadline->stopCalls > 0);

    job.cancel();
    QVERIFY(!job.isActive());
    staleBackendError(VideoThumbnailBackendError::Other, {});
    staleDeadlineExpiry();
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 0);
}

void VideoThumbnailExtractionLifecycleTest::activeCancellationReleasesResources()
{
    QObject receiver;
    ExtractionHarness harness;
    int completionCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&completionCount](VideoThumbnailExtractionResult) { ++completionCount; },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();
    QVERIFY(job.isActive());
    QCOMPARE(harness.backend->setSourceCalls, 1);

    job.cancel();
    QVERIFY(!job.isActive());
    QCOMPARE(harness.backend->stopCalls, 1);
    QCOMPARE(harness.deadline->stopCalls, 1);
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 0);
}

void VideoThumbnailExtractionLifecycleTest::physicalRetirementFollowsBackendDestruction()
{
    QObject receiver;
    ExtractionHarness harness;
    int retirementCount = 0;
    bool backendDestroyedAtRetirement = false;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(), [](VideoThumbnailExtractionResult) { },
        harness.dependencies());
    job.setRetirementCallback([&]() {
        ++retirementCount;
        backendDestroyedAtRetirement = harness.backend->destructions == 1;
    });
    kiriview::test::drainQueuedCalls();

    QVERIFY(job.isActive());
    QCOMPARE(harness.backend->creations, 1);
    job.cancel();

    QVERIFY(!job.isActive());
    QCOMPARE(harness.backend->stopCalls, 1);
    QCOMPARE(harness.backend->destructions, 0);
    QCOMPARE(retirementCount, 0);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QCOMPARE(harness.backend->destructions, 1);
    QCOMPARE(harness.deadline->destructions, 1);
    QCOMPARE(retirementCount, 1);
    QVERIFY(backendDestroyedAtRetirement);
}

void VideoThumbnailExtractionLifecycleTest::receiverDestructionWhileActiveSuppressesDelivery()
{
    auto receiver = std::make_unique<QObject>();
    ExtractionHarness harness;
    int completionCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        receiver.get(), kiriview::test::validRequest(),
        [&completionCount](VideoThumbnailExtractionResult) { ++completionCount; },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();
    QVERIFY(job.isActive());

    receiver.reset();
    QVERIFY(!job.isActive());
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 0);
    QCOMPARE(harness.backend->stopCalls, 1);
    QCOMPARE(harness.deadline->stopCalls, 1);
}

void VideoThumbnailExtractionLifecycleTest::receiverDestructionSuppressesPendingDelivery()
{
    auto receiver = std::make_unique<QObject>();
    ExtractionHarness harness;
    int completionCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        receiver.get(), kiriview::test::validRequest(),
        [&completionCount](VideoThumbnailExtractionResult) { ++completionCount; },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    const auto staleBackendError = harness.backend->instance->errorCallback();
    const auto staleDeadlineExpiry = harness.deadline->expired;
    QVERIFY(staleBackendError);
    QVERIFY(staleDeadlineExpiry);
    QImage cover(8, 8, QImage::Format_RGBA8888);
    cover.fill(Qt::yellow);
    harness.backend->instance->emitMetadata(VideoThumbnailEmbeddedImages { cover, {} });

    QVERIFY(job.isActive());
    QCOMPARE(completionCount, 0);
    QVERIFY(harness.backend->stopCalls > 0);
    QVERIFY(harness.deadline->stopCalls > 0);

    receiver.reset();
    QVERIFY(!job.isActive());
    staleBackendError(VideoThumbnailBackendError::Other, {});
    staleDeadlineExpiry();
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 0);
}

void VideoThumbnailExtractionLifecycleTest::firstTerminalOutcomeWinsAcrossReentrantCleanup()
{
    QObject receiver;
    ExtractionHarness harness;
    harness.backend->emitErrorFromStop = true;
    int completionCount = 0;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&](VideoThumbnailExtractionResult value) {
            ++completionCount;
            result = std::move(value);
        },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    QImage cover(8, 8, QImage::Format_RGBA8888);
    cover.fill(Qt::yellow);
    harness.backend->instance->emitMetadata(VideoThumbnailEmbeddedImages { cover, {} });
    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 1);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QVERIFY(!result->failure.has_value());
    QVERIFY(!job.isActive());
}

void VideoThumbnailExtractionLifecycleTest::moveAssignmentCancelsReplacedOperation()
{
    QObject firstReceiver;
    QObject secondReceiver;
    ExtractionHarness firstHarness;
    ExtractionHarness secondHarness;
    int firstCompletionCount = 0;
    int secondCompletionCount = 0;

    auto first = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &firstReceiver, {},
        [&firstCompletionCount](VideoThumbnailExtractionResult) { ++firstCompletionCount; },
        firstHarness.dependencies());
    auto second = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &secondReceiver, {},
        [&secondCompletionCount](VideoThumbnailExtractionResult) { ++secondCompletionCount; },
        secondHarness.dependencies());

    first = std::move(second);
    QVERIFY(first.isActive());
    QVERIFY(!second.isActive());
    kiriview::test::drainQueuedCalls();

    QCOMPARE(firstCompletionCount, 0);
    QCOMPARE(secondCompletionCount, 1);
    QVERIFY(!first.isActive());
}

void VideoThumbnailExtractionLifecycleTest::callbackMayReleaseJobAndReceiver()
{
    auto receiver = std::make_unique<QObject>();
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionJob> job;
    int completionCount = 0;

    job.emplace(kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        receiver.get(), {},
        [&](VideoThumbnailExtractionResult) {
            ++completionCount;
            job.reset();
            receiver.reset();
        },
        harness.dependencies()));

    kiriview::test::drainQueuedCalls();

    QCOMPARE(completionCount, 1);
    QVERIFY(!job.has_value());
    QVERIFY(!receiver);
}

} // namespace

QTEST_GUILESS_MAIN(VideoThumbnailExtractionLifecycleTest)

#include "tst_videothumbnailextraction_lifecycle.moc"
