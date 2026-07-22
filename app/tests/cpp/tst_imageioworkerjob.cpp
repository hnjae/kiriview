// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imageioworkerjob.h"
#include "image_async_test_support.h"

#include <QSemaphore>
#include <QTest>
#include <QThreadPool>
#include <memory>
#include <vector>

namespace {
kiriview::ImageWorkerScheduler workerSchedulerForPool(QThreadPool* pool)
{
    return kiriview::ImageWorkerScheduler(
        [pool](QObject* context, kiriview::ImageWorkerOperation work,
            kiriview::ImageWorkerCompletion completion) {
            return kiriview::runAsyncWorker(
                pool, context,
                [work = std::move(work)]() mutable {
                    work();
                    return true;
                },
                [completion = std::move(completion)](bool) mutable { completion(); });
        });
}
}

class TestImageIoWorkerJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nullReceiverRunsSynchronously();
    void moveOnlyWorkAndCompletionAreSupported();
    void workerCompletionFinishesJobOnce();
    void canceledWorkerCompletionIsIgnored();
    void canceledQueuedWorkerReleasesPayloadWithoutRunning();
    void supersededThreadPoolBacklogIsWithdrawnBeforeNewestWork();
};

void TestImageIoWorkerJob::nullReceiverRunsSynchronously()
{
    int workCount = 0;
    int finishValue = 0;

    kiriview::ImageIoJob job = kiriview::startImageIoWorkerJob(
        nullptr,
        [&workCount]() {
            ++workCount;
            return 7;
        },
        [&finishValue](int value) { finishValue = value; });

    QVERIFY(!job.isActive());
    QCOMPARE(workCount, 1);
    QCOMPARE(finishValue, 7);
}

void TestImageIoWorkerJob::moveOnlyWorkAndCompletionAreSupported()
{
    auto workPayload = std::make_unique<int>(17);
    auto completionPayload = std::make_unique<int>(5);
    int finishedValue = 0;

    kiriview::ImageIoJob job = kiriview::startImageIoWorkerJob(
        nullptr, [payload = std::move(workPayload)]() { return *payload; },
        [payload = std::move(completionPayload), &finishedValue](
            int value) { finishedValue = value + *payload; });

    QVERIFY(!job.isActive());
    QCOMPARE(finishedValue, 22);
}

void TestImageIoWorkerJob::workerCompletionFinishesJobOnce()
{
    QObject receiver;
    int finishCount = 0;
    int finishValue = 0;

    kiriview::ImageIoJob job = kiriview::startImageIoWorkerJob(
        &receiver, []() { return 11; },
        [&finishCount, &finishValue](int value) {
            ++finishCount;
            finishValue = value;
        });

    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(finishValue, 11);
    QVERIFY(!job.isActive());
}

void TestImageIoWorkerJob::canceledWorkerCompletionIsIgnored()
{
    QObject context;
    QObject receiver;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    int finishCount = 0;

    kiriview::ImageIoJob job = kiriview::startImageIoWorkerJob(
        &context, &receiver, workerScheduler.scheduler(), []() { return 13; },
        [&finishCount](int) { ++finishCount; });

    QVERIFY(job.isActive());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));

    job.cancel();
    QVERIFY(!job.isActive());

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(finishCount, 0);
}

void TestImageIoWorkerJob::canceledQueuedWorkerReleasesPayloadWithoutRunning()
{
    QObject context;
    QObject receiver;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    auto payload = std::make_shared<QByteArray>(1024, 'x');
    const std::weak_ptr<QByteArray> retainedPayload = payload;
    int workCount = 0;

    kiriview::ImageIoJob job = kiriview::startImageIoWorkerJob(
        &context, &receiver, workerScheduler.scheduler(),
        [payload = std::move(payload), &workCount]() {
            ++workCount;
            return payload->size();
        },
        [](qsizetype) {});

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QVERIFY(!retainedPayload.expired());

    job.cancel();

    QVERIFY(retainedPayload.expired());
    workerScheduler.runWork(0);
    QCOMPARE(workCount, 0);
}

void TestImageIoWorkerJob::supersededThreadPoolBacklogIsWithdrawnBeforeNewestWork()
{
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    QSemaphore blockerStarted;
    QSemaphore releaseBlocker;
    pool.start(QRunnable::create([&]() {
        blockerStarted.release();
        releaseBlocker.acquire();
    }));
    QVERIFY(blockerStarted.tryAcquire(1, 1000));

    QObject context;
    QObject receiver;
    constexpr int obsoleteJobCount = 6;
    std::vector<kiriview::ImageIoJob> obsoleteJobs;
    std::vector<std::weak_ptr<QByteArray>> retainedPayloads;
    int obsoleteWorkCount = 0;
    for (int index = 0; index < obsoleteJobCount; ++index) {
        auto payload = std::make_shared<QByteArray>(1024 * (index + 1), 'x');
        retainedPayloads.emplace_back(payload);
        obsoleteJobs.push_back(kiriview::startImageIoWorkerJob(
            &context, &receiver, workerSchedulerForPool(&pool),
            [payload = std::move(payload), &obsoleteWorkCount]() {
                ++obsoleteWorkCount;
                return payload->size();
            },
            [](qsizetype) {}));
    }
    for (const auto& retainedPayload : retainedPayloads) {
        QVERIFY(!retainedPayload.expired());
    }
    for (auto& job : obsoleteJobs) {
        job.cancel();
    }
    for (const auto& retainedPayload : retainedPayloads) {
        QVERIFY(retainedPayload.expired());
    }

    int newestWorkCount = 0;
    int newestFinishCount = 0;
    kiriview::ImageIoJob newestJob = kiriview::startImageIoWorkerJob(
        &context, &receiver, workerSchedulerForPool(&pool),
        [&newestWorkCount]() {
            ++newestWorkCount;
            return 17;
        },
        [&newestFinishCount](int) { ++newestFinishCount; });

    releaseBlocker.release();
    QVERIFY(pool.waitForDone(1000));
    QTRY_COMPARE(newestFinishCount, 1);
    QCOMPARE(obsoleteWorkCount, 0);
    QCOMPARE(newestWorkCount, 1);
    QVERIFY(!newestJob.isActive());
}

QTEST_GUILESS_MAIN(TestImageIoWorkerJob)

#include "tst_imageioworkerjob.moc"
