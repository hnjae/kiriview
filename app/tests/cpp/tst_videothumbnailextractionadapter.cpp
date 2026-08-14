// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/videothumbnailextractionadapter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QTest>

#include <memory>

namespace {
void drainQueuedCalls()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}
}

class TestVideoThumbnailExtractionAdapter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void invalidRequestCompletesThroughApplicationJob();
    void cancellationSuppressesPendingLibraryDelivery();
    void receiverDestructionSuppressesPendingLibraryDelivery();
};

void TestVideoThumbnailExtractionAdapter::invalidRequestCompletesThroughApplicationJob()
{
    QObject receiver;
    int completionCount = 0;
    kiriview::VideoThumbnailExtractionResult delivered;

    kiriview::ImageIoJob job = kiriview::startThumbnailVideoExtractionJob(
        &receiver, {}, [&](kiriview::VideoThumbnailExtractionResult result) {
            ++completionCount;
            delivered = std::move(result);
        });

    QVERIFY(job.isActive());
    QCOMPARE(completionCount, 0);

    drainQueuedCalls();

    QCOMPARE(completionCount, 1);
    QVERIFY(!job.isActive());
    QCOMPARE(delivered.status, kiriview::VideoThumbnailExtractionStatus::Failed);
    QVERIFY(delivered.failure.has_value());
    QCOMPARE(
        delivered.failure->cause, kiriview::VideoThumbnailExtractionFailureCause::InvalidRequest);
}

void TestVideoThumbnailExtractionAdapter::cancellationSuppressesPendingLibraryDelivery()
{
    QObject receiver;
    int completionCount = 0;
    int retirementCount = 0;

    kiriview::ImageIoJob job = kiriview::startThumbnailVideoExtractionJob(
        &receiver, {}, [&](kiriview::VideoThumbnailExtractionResult) { ++completionCount; });
    job.setRetirementCallback([&retirementCount]() { ++retirementCount; });

    QVERIFY(job.isActive());
    job.cancel();
    QVERIFY(!job.isActive());
    QCOMPARE(retirementCount, 0);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    drainQueuedCalls();

    QCOMPARE(completionCount, 0);
    QCOMPARE(retirementCount, 1);
}

void TestVideoThumbnailExtractionAdapter::receiverDestructionSuppressesPendingLibraryDelivery()
{
    auto receiver = std::make_unique<QObject>();
    int completionCount = 0;

    kiriview::ImageIoJob job = kiriview::startThumbnailVideoExtractionJob(
        receiver.get(), {}, [&](kiriview::VideoThumbnailExtractionResult) { ++completionCount; });

    QVERIFY(job.isActive());
    receiver.reset();
    QVERIFY(!job.isActive());

    drainQueuedCalls();

    QCOMPARE(completionCount, 0);
}

QTEST_GUILESS_MAIN(TestVideoThumbnailExtractionAdapter)

#include "tst_videothumbnailextractionadapter.moc"
