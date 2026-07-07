// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imageioworkerjob.h"
#include "image_async_test_support.h"

#include <QTest>

class TestImageIoWorkerJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nullReceiverRunsSynchronously();
    void workerCompletionFinishesJobOnce();
    void canceledWorkerCompletionIsIgnored();
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

QTEST_GUILESS_MAIN(TestImageIoWorkerJob)

#include "test_imageioworkerjob.moc"
