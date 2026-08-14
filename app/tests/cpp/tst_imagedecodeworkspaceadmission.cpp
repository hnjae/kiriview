// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodeworkspace.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QScopeGuard>
#include <QTest>
#include <QThread>
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <optional>
#include <thread>
#include <vector>

namespace {
using kiriview::ImageDecodeWorkspaceAdmission;
using kiriview::ImageDecodeWorkspaceAdmissionFailure;
using kiriview::ImageDecodeWorkspaceAdmissionRequest;
using kiriview::ImageDecodeWorkspaceBudget;
using kiriview::ImageDecodeWorkspaceLease;
using kiriview::ImageDecodeWorkspacePriority;
namespace WorkspaceInternal = kiriview::ImageDecodeWorkspaceDetail;
}

class TestImageDecodeWorkspaceAdmission : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsInvalidAndIntrinsicHardLimitViolations();
    void feasibleAggregateContentionRemainsPending();
    void grantsHigherPrioritiesFirst();
    void preservesEqualPriorityFifoWithoutBypass();
    void cancelingPendingAdmissionPreventsGrant();
    void deletingReceiverReleasesQueuedGrant();
    void workerThreadReleaseRepumpsToReceiverThread();
    void pendingGrantFollowsReceiverAffinityChange();
    void retainedAliasesBlockUntilFinalRetirement();
    void bestEffortAdmissionDoesNotBypassEqualOrHigherWaiters();
    void prechargedBudgetReleasesTransientPeakAtFinalization();
    void foregroundPrecedesFurtherSpeculationAfterFirstRetirement();
};

void TestImageDecodeWorkspaceAdmission::rejectsInvalidAndIntrinsicHardLimitViolations()
{
    ImageDecodeWorkspaceBudget budget(100, 60);
    QObject receiver;
    const auto callback = [](ImageDecodeWorkspaceLease) { };

    auto invalid = budget.requestAdmission(&receiver,
        { std::numeric_limits<qsizetype>::max(), 1, ImageDecodeWorkspacePriority::Interactive },
        callback);
    QVERIFY(!invalid.has_value());
    QCOMPARE(invalid.error(), ImageDecodeWorkspaceAdmissionFailure::InvalidRequest);

    auto perOperation = budget.requestAdmission(
        &receiver, { 31, 30, ImageDecodeWorkspacePriority::Interactive }, callback);
    QVERIFY(!perOperation.has_value());
    QCOMPARE(perOperation.error(), ImageDecodeWorkspaceAdmissionFailure::PerOperationLimitExceeded);

    auto aggregate = budget.requestAdmission(
        &receiver, { 101, 0, ImageDecodeWorkspacePriority::Interactive }, callback);
    QVERIFY(!aggregate.has_value());
    QCOMPARE(aggregate.error(), ImageDecodeWorkspaceAdmissionFailure::AggregateLimitExceeded);

    auto missingReceiver = budget.requestAdmission(
        nullptr, { 1, 0, ImageDecodeWorkspacePriority::Interactive }, callback);
    QVERIFY(!missingReceiver.has_value());
    QCOMPARE(missingReceiver.error(), ImageDecodeWorkspaceAdmissionFailure::InvalidRequest);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::feasibleAggregateContentionRemainsPending()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 10));

    QObject receiver;
    int grantCount = 0;
    auto requested
        = budget.requestAdmission(&receiver, { 6, 0, ImageDecodeWorkspacePriority::Interactive },
            [&grantCount](ImageDecodeWorkspaceLease lease) {
                QCOMPARE(lease.reservedByteCount(), qsizetype(6));
                ++grantCount;
            });
    QVERIFY(requested.has_value());
    QVERIFY(requested->isPending());
    QCoreApplication::processEvents();
    QCOMPARE(grantCount, 0);
    QCOMPARE(budget.reservedByteCount(), qsizetype(10));

    QVERIFY(blocker.release(10));
    QTRY_COMPARE(grantCount, 1);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::grantsHigherPrioritiesFirst()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 10));

    QObject receiver;
    std::vector<ImageDecodeWorkspacePriority> grantOrder;
    std::vector<ImageDecodeWorkspaceAdmission> admissions;
    const auto request = [&](ImageDecodeWorkspacePriority priority) {
        auto admission = budget.requestAdmission(&receiver, { 10, 0, priority },
            [&grantOrder, priority](ImageDecodeWorkspaceLease) { grantOrder.push_back(priority); });
        QVERIFY(admission.has_value());
        QVERIFY(admission->isPending());
        admissions.push_back(std::move(*admission));
    };

    request(ImageDecodeWorkspacePriority::Speculative);
    request(ImageDecodeWorkspacePriority::Demanded);
    request(ImageDecodeWorkspacePriority::Interactive);
    QVERIFY(blocker.release(10));

    QTRY_COMPARE(grantOrder.size(), std::size_t(3));
    QCOMPARE(grantOrder.at(0), ImageDecodeWorkspacePriority::Interactive);
    QCOMPARE(grantOrder.at(1), ImageDecodeWorkspacePriority::Demanded);
    QCOMPARE(grantOrder.at(2), ImageDecodeWorkspacePriority::Speculative);
}

void TestImageDecodeWorkspaceAdmission::preservesEqualPriorityFifoWithoutBypass()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 5));

    QObject receiver;
    std::vector<int> grantOrder;
    std::optional<ImageDecodeWorkspaceLease> firstLease;
    auto first
        = budget.requestAdmission(&receiver, { 6, 0, ImageDecodeWorkspacePriority::Demanded },
            [&grantOrder, &firstLease](ImageDecodeWorkspaceLease lease) {
                grantOrder.push_back(1);
                firstLease.emplace(std::move(lease));
            });
    auto second
        = budget.requestAdmission(&receiver, { 5, 0, ImageDecodeWorkspacePriority::Demanded },
            [&grantOrder](ImageDecodeWorkspaceLease) { grantOrder.push_back(2); });
    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QCoreApplication::processEvents();
    QVERIFY(grantOrder.empty());

    QVERIFY(blocker.release(5));
    QTRY_COMPARE(grantOrder.size(), std::size_t(1));
    QCOMPARE(grantOrder.front(), 1);
    QVERIFY(second->isPending());
    QCOMPARE(budget.reservedByteCount(), qsizetype(6));

    firstLease.reset();
    QTRY_COMPARE(grantOrder.size(), std::size_t(2));
    QCOMPARE(grantOrder.back(), 2);
}

void TestImageDecodeWorkspaceAdmission::cancelingPendingAdmissionPreventsGrant()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 10));

    QObject receiver;
    int grantCount = 0;
    auto admission
        = budget.requestAdmission(&receiver, { 10, 0, ImageDecodeWorkspacePriority::Interactive },
            [&grantCount](ImageDecodeWorkspaceLease) { ++grantCount; });
    QVERIFY(admission.has_value());
    QVERIFY(admission->isPending());
    admission->cancel();
    QVERIFY(!admission->isPending());

    QVERIFY(blocker.release(10));
    QCoreApplication::processEvents();
    QCOMPARE(grantCount, 0);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::deletingReceiverReleasesQueuedGrant()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    auto* firstReceiver = new QObject;
    QObject secondReceiver;
    int firstGrantCount = 0;
    int secondGrantCount = 0;

    auto first = budget.requestAdmission(firstReceiver,
        { 10, 0, ImageDecodeWorkspacePriority::Interactive },
        [&firstGrantCount](ImageDecodeWorkspaceLease) { ++firstGrantCount; });
    auto second = budget.requestAdmission(&secondReceiver,
        { 10, 0, ImageDecodeWorkspacePriority::Interactive },
        [&secondGrantCount](ImageDecodeWorkspaceLease) { ++secondGrantCount; });
    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(!first->isPending());
    QVERIFY(second->isPending());
    QCOMPARE(budget.reservedByteCount(), qsizetype(10));

    delete firstReceiver;
    QTRY_COMPARE(secondGrantCount, 1);
    QCOMPARE(firstGrantCount, 0);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::workerThreadReleaseRepumpsToReceiverThread()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 10));

    QObject receiver;
    QThread* callbackThread = nullptr;
    int grantCount = 0;
    auto admission
        = budget.requestAdmission(&receiver, { 10, 0, ImageDecodeWorkspacePriority::Interactive },
            [&callbackThread, &grantCount](ImageDecodeWorkspaceLease) {
                callbackThread = QThread::currentThread();
                ++grantCount;
            });
    QVERIFY(admission.has_value());
    QVERIFY(admission->isPending());

    std::thread releaser([lease = std::move(blocker)]() mutable { lease = {}; });
    releaser.join();
    QTRY_COMPARE(grantCount, 1);
    QCOMPARE(callbackThread, receiver.thread());
}

void TestImageDecodeWorkspaceAdmission::pendingGrantFollowsReceiverAffinityChange()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 10));

    QThread receiverThread;
    receiverThread.start();
    QThread* testThread = QThread::currentThread();
    QObject receiver;
    const auto stopReceiverThread = qScopeGuard([&]() {
        if (receiver.thread() == &receiverThread && receiverThread.isRunning()) {
            QMetaObject::invokeMethod(
                &receiver,
                [&receiver, testThread]() { static_cast<void>(receiver.moveToThread(testThread)); },
                Qt::BlockingQueuedConnection);
        }
        receiverThread.quit();
        receiverThread.wait();
    });

    std::atomic<QThread*> callbackThread = nullptr;
    std::atomic_int grantCount = 0;
    auto admission
        = budget.requestAdmission(&receiver, { 10, 0, ImageDecodeWorkspacePriority::Interactive },
            [&callbackThread, &grantCount](ImageDecodeWorkspaceLease) {
                callbackThread.store(QThread::currentThread(), std::memory_order_relaxed);
                grantCount.fetch_add(1, std::memory_order_release);
            });
    QVERIFY(admission.has_value());
    QVERIFY(admission->isPending());
    QVERIFY(receiver.moveToThread(&receiverThread));

    QVERIFY(blocker.release(10));
    QTRY_COMPARE(grantCount.load(std::memory_order_acquire), 1);
    QCOMPARE(callbackThread.load(std::memory_order_relaxed), &receiverThread);
    QTRY_COMPARE(budget.reservedByteCount(), qsizetype(0));

    bool movedBack = false;
    QVERIFY(QMetaObject::invokeMethod(
        &receiver,
        [&receiver, testThread, &movedBack]() { movedBack = receiver.moveToThread(testThread); },
        Qt::BlockingQueuedConnection));
    QVERIFY(movedBack);
    receiverThread.quit();
    QVERIFY(receiverThread.wait(1000));
}

void TestImageDecodeWorkspaceAdmission::retainedAliasesBlockUntilFinalRetirement()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease producer = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(producer, 10));
    kiriview::ImageDecodeWorkspaceHold retained = producer.retainOnly(10);
    kiriview::ImageDecodeWorkspaceHold retainedAlias = retained;

    QObject receiver;
    int grantCount = 0;
    auto admission
        = budget.requestAdmission(&receiver, { 10, 0, ImageDecodeWorkspacePriority::Interactive },
            [&grantCount](ImageDecodeWorkspaceLease) { ++grantCount; });
    QVERIFY(admission.has_value());
    QVERIFY(admission->isPending());

    retained = {};
    QCoreApplication::processEvents();
    QCOMPARE(grantCount, 0);
    QCOMPARE(budget.reservedByteCount(), qsizetype(10));

    retainedAlias = {};
    QTRY_COMPARE(grantCount, 1);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::bestEffortAdmissionDoesNotBypassEqualOrHigherWaiters()
{
    ImageDecodeWorkspaceBudget budget(10, 10);
    ImageDecodeWorkspaceLease blocker = WorkspaceInternal::startLease(budget);
    QVERIFY(WorkspaceInternal::tryReserve(blocker, 6));

    QObject receiver;
    auto demanded = budget.requestAdmission(&receiver,
        { 5, 0, ImageDecodeWorkspacePriority::Demanded }, [](ImageDecodeWorkspaceLease) { });
    QVERIFY(demanded.has_value());
    QVERIFY(demanded->isPending());

    QVERIFY(!WorkspaceInternal::tryBestEffortAdmission(
        budget, { 1, 0, ImageDecodeWorkspacePriority::Demanded })
            .has_value());
    QVERIFY(!WorkspaceInternal::tryBestEffortAdmission(
        budget, { 1, 0, ImageDecodeWorkspacePriority::Speculative })
            .has_value());

    std::optional<ImageDecodeWorkspaceLease> higherPriority
        = WorkspaceInternal::tryBestEffortAdmission(
            budget, { 1, 0, ImageDecodeWorkspacePriority::Interactive });
    QVERIFY(higherPriority.has_value());
    QCOMPARE(higherPriority->reservedByteCount(), qsizetype(1));
    higherPriority.reset();

    QVERIFY(!WorkspaceInternal::tryBestEffortAdmission(
        budget, { 5, 0, ImageDecodeWorkspacePriority::Interactive })
            .has_value());
    demanded->cancel();
}

void TestImageDecodeWorkspaceAdmission::prechargedBudgetReleasesTransientPeakAtFinalization()
{
    ImageDecodeWorkspaceBudget budget(100, 100);
    QObject receiver;
    std::shared_ptr<ImageDecodeWorkspaceBudget> localBudget;
    auto stage
        = budget.requestAdmission(&receiver, { 80, 20, ImageDecodeWorkspacePriority::Interactive },
            [&localBudget](ImageDecodeWorkspaceLease grant) {
                localBudget = kiriview::prechargedImageDecodeWorkspaceBudget(std::move(grant), 20);
            });
    QVERIFY(stage.has_value());
    QTRY_VERIFY(localBudget != nullptr);
    QCOMPARE(localBudget->aggregateByteLimit(), qsizetype(80));
    QCOMPARE(localBudget->perOperationByteLimit(), qsizetype(100));
    QCOMPARE(budget.reservedByteCount(), qsizetype(80));

    ImageDecodeWorkspaceLease transient
        = WorkspaceInternal::startLeaseForOperation(*localBudget, 20);
    ImageDecodeWorkspaceLease output = WorkspaceInternal::startLease(*localBudget);
    QVERIFY(WorkspaceInternal::tryReserve(transient, 60));
    QVERIFY(WorkspaceInternal::tryReserve(output, 20));
    QVERIFY(!WorkspaceInternal::tryReserve(output, 1));
    transient = {};
    QCOMPARE(localBudget->reservedByteCount(), qsizetype(20));
    QCOMPARE(budget.reservedByteCount(), qsizetype(80));

    localBudget->finalizePrechargedAdmission();
    QCOMPARE(budget.reservedByteCount(), qsizetype(20));
    ImageDecodeWorkspaceLease lateLease = WorkspaceInternal::startLease(*localBudget);
    QVERIFY(!WorkspaceInternal::tryReserve(lateLease, 1));

    kiriview::ImageDecodeWorkspaceHold outputHold = output.retainOnly(20);
    int followingGrantCount = 0;
    auto following
        = budget.requestAdmission(&receiver, { 90, 0, ImageDecodeWorkspacePriority::Interactive },
            [&followingGrantCount](ImageDecodeWorkspaceLease) { ++followingGrantCount; });
    QVERIFY(following.has_value());
    QVERIFY(following->isPending());

    outputHold = {};
    QTRY_COMPARE(followingGrantCount, 1);
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

void TestImageDecodeWorkspaceAdmission::foregroundPrecedesFurtherSpeculationAfterFirstRetirement()
{
    kiriview::ImageDecodeWorkspaceBudget budget(2048, 1024);
    QObject receiver;
    std::array<std::optional<kiriview::ImageDecodeWorkspaceLease>, 3> runningSpeculative;
    std::vector<kiriview::ImageDecodeWorkspaceAdmission> admissions;
    for (std::size_t index = 0; index < runningSpeculative.size(); ++index) {
        auto admission = budget.requestAdmission(&receiver,
            { 610, 0, kiriview::ImageDecodeWorkspacePriority::Speculative },
            [&runningSpeculative, index](kiriview::ImageDecodeWorkspaceLease lease) {
                runningSpeculative.at(index).emplace(std::move(lease));
            });
        QVERIFY(admission.has_value());
        admissions.push_back(std::move(*admission));
    }
    QTRY_VERIFY(std::ranges::all_of(
        runningSpeculative, [](const auto& lease) { return lease.has_value(); }));
    QCOMPARE(budget.reservedByteCount(), qsizetype(1830));

    std::vector<kiriview::ImageDecodeWorkspacePriority> grantOrder;
    std::optional<kiriview::ImageDecodeWorkspaceLease> foregroundLease;
    auto foreground = budget.requestAdmission(&receiver,
        { 610, 0, kiriview::ImageDecodeWorkspacePriority::Interactive },
        [&grantOrder, &foregroundLease](kiriview::ImageDecodeWorkspaceLease lease) {
            grantOrder.push_back(kiriview::ImageDecodeWorkspacePriority::Interactive);
            foregroundLease.emplace(std::move(lease));
        });
    QVERIFY(foreground.has_value());
    admissions.push_back(std::move(*foreground));

    std::optional<kiriview::ImageDecodeWorkspaceLease> laterSpeculativeLease;
    auto laterSpeculative = budget.requestAdmission(&receiver,
        { 100, 0, kiriview::ImageDecodeWorkspacePriority::Speculative },
        [&grantOrder, &laterSpeculativeLease](kiriview::ImageDecodeWorkspaceLease lease) {
            grantOrder.push_back(kiriview::ImageDecodeWorkspacePriority::Speculative);
            laterSpeculativeLease.emplace(std::move(lease));
        });
    QVERIFY(laterSpeculative.has_value());
    admissions.push_back(std::move(*laterSpeculative));
    QCoreApplication::processEvents();
    QVERIFY(grantOrder.empty());

    runningSpeculative.front().reset();
    QTRY_COMPARE(grantOrder.size(), std::size_t(2));
    QCOMPARE(grantOrder.front(), kiriview::ImageDecodeWorkspacePriority::Interactive);
    QCOMPARE(grantOrder.back(), kiriview::ImageDecodeWorkspacePriority::Speculative);
    QVERIFY(foregroundLease.has_value());
    QVERIFY(laterSpeculativeLease.has_value());
}

QTEST_GUILESS_MAIN(TestImageDecodeWorkspaceAdmission)

#include "tst_imagedecodeworkspaceadmission.moc"
