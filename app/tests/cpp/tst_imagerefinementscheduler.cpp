// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imagerefinementscheduler.h"
#include "decoding/imagedecodedependencies.h"

#include <QSemaphore>
#include <QTest>
#include <QThread>
#include <atomic>

class TestImageRefinementScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cancelledRunningWorkRetainsAdmissionUntilPhysicalRetirement();
    void cancelledQueuedWorkPublishesPhysicalRetirement();
    void successfulWorkRetainsAdmissionThroughResultHandoff();
    void successfulWorkPublishesPhysicalRetirement();
    void callbackInstalledAfterRetirementRunsOnce();
    void destroyedReceiverSuppressesDeliveryAndPublishesRetirement();
    void stoppedReceiverThreadSuppressesDeliveryAndPublishesRetirement();
    void decodeDefaultsDoNotReuseTheGeneralWorkerScheduler();
};

void TestImageRefinementScheduler::cancelledRunningWorkRetainsAdmissionUntilPhysicalRetirement()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QObject context;
    QSemaphore firstStarted;
    QSemaphore releaseFirst;
    QSemaphore secondStarted;
    QSemaphore releaseSecond;
    bool firstFinished = false;
    bool secondFinished = false;
    int firstRetirementCount = 0;

    kiriview::ImageWorkerTask first = scheduler.run(
        &context,
        [&]() {
            firstStarted.release();
            releaseFirst.acquire();
            return 1;
        },
        [&](int) { firstFinished = true; });
    QVERIFY(firstStarted.tryAcquire(1, 2000));
    first.setRetirementCallback([&firstRetirementCount]() { ++firstRetirementCount; });
    first.cancel();
    QCOMPARE(firstRetirementCount, 0);

    kiriview::ImageWorkerTask second = scheduler.run(
        &context,
        [&]() {
            secondStarted.release();
            releaseSecond.acquire();
            return 2;
        },
        [&](int) { secondFinished = true; });
    QVERIFY(!secondStarted.tryAcquire());

    releaseFirst.release();
    QTRY_VERIFY_WITH_TIMEOUT(secondStarted.available() == 1, 2000);
    secondStarted.acquire();
    QTRY_COMPARE_WITH_TIMEOUT(firstRetirementCount, 1, 2000);
    QVERIFY(!firstFinished);
    QVERIFY(!secondFinished);

    releaseSecond.release();
    QTRY_VERIFY_WITH_TIMEOUT(secondFinished, 2000);
    QVERIFY(!firstFinished);
    QVERIFY(!first.isActive());
    QVERIFY(!second.isActive());
}

void TestImageRefinementScheduler::cancelledQueuedWorkPublishesPhysicalRetirement()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QObject context;
    QSemaphore firstStarted;
    QSemaphore releaseFirst;
    int queuedWorkCount = 0;
    int retirementCount = 0;

    kiriview::ImageWorkerTask first = scheduler.run(
        &context,
        [&]() {
            firstStarted.release();
            releaseFirst.acquire();
            return 1;
        },
        [](int) {});
    QVERIFY(firstStarted.tryAcquire(1, 2000));

    kiriview::ImageWorkerTask queued = scheduler.run(
        &context,
        [&queuedWorkCount]() {
            ++queuedWorkCount;
            return 2;
        },
        [](int) {});
    queued.setRetirementCallback([&retirementCount]() { ++retirementCount; });
    queued.cancel();

    QCOMPARE(retirementCount, 1);
    QCOMPARE(queuedWorkCount, 0);
    releaseFirst.release();
    QTRY_VERIFY_WITH_TIMEOUT(!first.isActive(), 2000);
    QCOMPARE(retirementCount, 1);
}

void TestImageRefinementScheduler::successfulWorkRetainsAdmissionThroughResultHandoff()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QObject context;
    QSemaphore firstStarted;
    QSemaphore releaseFirst;
    QSemaphore secondStarted;
    bool firstFinishedBeforeSecondStarted = false;
    bool secondFinished = false;

    kiriview::ImageWorkerTask first = scheduler.run(
        &context,
        [&]() {
            firstStarted.release();
            releaseFirst.acquire();
            return 1;
        },
        [&](int) { firstFinishedBeforeSecondStarted = secondStarted.available() == 0; });
    QVERIFY(firstStarted.tryAcquire(1, 2000));

    kiriview::ImageWorkerTask second = scheduler.run(
        &context,
        [&]() {
            secondStarted.release();
            return 2;
        },
        [&](int) { secondFinished = true; });
    QVERIFY(!secondStarted.tryAcquire());

    releaseFirst.release();
    QTRY_VERIFY_WITH_TIMEOUT(firstFinishedBeforeSecondStarted, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(secondFinished, 2000);
    QVERIFY(!first.isActive());
    QVERIFY(!second.isActive());
}

void TestImageRefinementScheduler::successfulWorkPublishesPhysicalRetirement()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QObject context;
    QSemaphore started;
    QSemaphore releaseWork;
    bool finished = false;
    int retirementCount = 0;

    kiriview::ImageWorkerTask task = scheduler.run(
        &context,
        [&]() {
            started.release();
            releaseWork.acquire();
            return 1;
        },
        [&](int) { finished = true; });
    task.setRetirementCallback([&retirementCount]() { ++retirementCount; });

    QVERIFY(started.tryAcquire(1, 2000));
    QCOMPARE(retirementCount, 0);
    releaseWork.release();
    QTRY_VERIFY_WITH_TIMEOUT(finished, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(retirementCount, 1, 2000);
}

void TestImageRefinementScheduler::callbackInstalledAfterRetirementRunsOnce()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QObject context;
    QSemaphore firstStarted;
    QSemaphore releaseFirst;
    QSemaphore secondStarted;
    QSemaphore releaseSecond;
    bool firstFinished = false;
    bool secondFinished = false;

    kiriview::ImageWorkerTask first = scheduler.run(
        &context,
        [&]() {
            firstStarted.release();
            releaseFirst.acquire();
            return 1;
        },
        [&](int) { firstFinished = true; });
    QVERIFY(firstStarted.tryAcquire(1, 2000));

    kiriview::ImageWorkerTask second = scheduler.run(
        &context,
        [&]() {
            secondStarted.release();
            releaseSecond.acquire();
            return 2;
        },
        [&](int) { secondFinished = true; });
    QVERIFY(!secondStarted.tryAcquire());

    releaseFirst.release();
    QTRY_VERIFY_WITH_TIMEOUT(firstFinished, 2000);
    QVERIFY(secondStarted.tryAcquire(1, 2000));

    int retirementCount = 0;
    first.setRetirementCallback([&retirementCount]() { ++retirementCount; });
    QCOMPARE(retirementCount, 1);

    releaseSecond.release();
    QTRY_VERIFY_WITH_TIMEOUT(secondFinished, 2000);
    QCOMPARE(retirementCount, 1);
}

void TestImageRefinementScheduler::destroyedReceiverSuppressesDeliveryAndPublishesRetirement()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    auto* context = new QObject;
    QSemaphore started;
    QSemaphore releaseWork;
    int finishCount = 0;
    std::atomic_int retirementCount = 0;

    kiriview::ImageWorkerTask task = scheduler.run(
        context,
        [&]() {
            started.release();
            releaseWork.acquire();
            return 1;
        },
        [&](int) { ++finishCount; });
    task.setRetirementCallback([&retirementCount]() { ++retirementCount; });

    QVERIFY(started.tryAcquire(1, 2000));
    delete context;
    releaseWork.release();

    QTRY_COMPARE_WITH_TIMEOUT(retirementCount.load(), 1, 2000);
    QCOMPARE(finishCount, 0);
    QVERIFY(!task.isActive());
}

void TestImageRefinementScheduler::stoppedReceiverThreadSuppressesDeliveryAndPublishesRetirement()
{
    kiriview::ImageWorkerScheduler scheduler = kiriview::defaultImageRefinementScheduler();
    QThread receiverThread;
    auto* context = new QObject;
    context->moveToThread(&receiverThread);
    QObject::connect(&receiverThread, &QThread::finished, context, &QObject::deleteLater);
    receiverThread.start();
    QSemaphore started;
    QSemaphore releaseWork;
    std::atomic_int finishCount = 0;
    std::atomic_int retirementCount = 0;

    kiriview::ImageWorkerTask task = scheduler.run(
        context,
        [&]() {
            started.release();
            releaseWork.acquire();
            return 1;
        },
        [&](int) { ++finishCount; });
    task.setRetirementCallback([&retirementCount]() { ++retirementCount; });

    QVERIFY(started.tryAcquire(1, 2000));
    receiverThread.quit();
    QVERIFY(receiverThread.wait(2000));
    releaseWork.release();

    QTRY_COMPARE_WITH_TIMEOUT(retirementCount.load(), 1, 2000);
    QCOMPARE(finishCount.load(), 0);
    QVERIFY(!task.isActive());
}

void TestImageRefinementScheduler::decodeDefaultsDoNotReuseTheGeneralWorkerScheduler()
{
    int generalScheduleCount = 0;
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.workerScheduler = kiriview::ImageWorkerScheduler(
        [&generalScheduleCount](
            QObject*, kiriview::ImageWorkerOperation, kiriview::ImageWorkerCompletion) {
            ++generalScheduleCount;
            return kiriview::ImageWorkerTask {};
        });
    dependencies = kiriview::imageDecodeDependenciesWithDefaults(std::move(dependencies));

    QObject context;
    QSemaphore started;
    bool finished = false;
    kiriview::ImageWorkerTask task = dependencies.refinementScheduler.run(
        &context,
        [&started]() {
            started.release();
            return 1;
        },
        [&finished](int) { finished = true; });

    QVERIFY(task.isActive());
    QVERIFY(started.tryAcquire(1, 2000));
    QTRY_VERIFY_WITH_TIMEOUT(finished, 2000);
    QCOMPARE(generalScheduleCount, 0);
    QVERIFY(!task.isActive());
}

QTEST_GUILESS_MAIN(TestImageRefinementScheduler)

#include "tst_imagerefinementscheduler.moc"
