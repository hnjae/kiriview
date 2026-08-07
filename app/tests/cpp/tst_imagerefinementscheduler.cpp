// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imagerefinementscheduler.h"
#include "decoding/imagedecodedependencies.h"

#include <QSemaphore>
#include <QTest>

class TestImageRefinementScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cancelledRunningWorkRetainsAdmissionUntilPhysicalRetirement();
    void successfulWorkRetainsAdmissionThroughResultHandoff();
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

    kiriview::ImageWorkerTask first = scheduler.run(
        &context,
        [&]() {
            firstStarted.release();
            releaseFirst.acquire();
            return 1;
        },
        [&](int) { firstFinished = true; });
    QVERIFY(firstStarted.tryAcquire(1, 2000));
    first.cancel();

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
    QVERIFY(!firstFinished);
    QVERIFY(!secondFinished);

    releaseSecond.release();
    QTRY_VERIFY_WITH_TIMEOUT(secondFinished, 2000);
    QVERIFY(!firstFinished);
    QVERIFY(!first.isActive());
    QVERIFY(!second.isActive());
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
