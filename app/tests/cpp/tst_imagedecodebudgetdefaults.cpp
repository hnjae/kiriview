// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodeworkspace.h"
#include "decoding/imagesourcedata.h"

#include <QTest>

class TestImageDecodeBudgetDefaults : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptedUnknownSystemMemorySnapshotIsNotReplaced();
    void retainedSplitPreservesAggregateUntilEachPhysicalOwnerRetires();
};

void TestImageDecodeBudgetDefaults::acceptedUnknownSystemMemorySnapshotIsNotReplaced()
{
    const kiriview::SystemMemorySnapshot unknownSystemMemory;
    const kiriview::ImageSourceDataBudgetLimits sourceDataLimits
        = kiriview::resolvedImageSourceDataBudgetLimits({}, unknownSystemMemory);
    const kiriview::ImageDecodeWorkspaceBudgetLimits workspaceLimits
        = kiriview::resolvedImageDecodeWorkspaceBudgetLimits({}, unknownSystemMemory);

    const std::shared_ptr<kiriview::ImageSourceDataBudget> sourceDataBudget
        = kiriview::imageSourceDataBudgetForSystemMemory({}, unknownSystemMemory);
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget
        = kiriview::imageDecodeWorkspaceBudgetForSystemMemory({}, unknownSystemMemory);

    QCOMPARE(sourceDataBudget->aggregateByteLimit(), sourceDataLimits.aggregateByteLimit);
    QCOMPARE(sourceDataBudget->perSourceByteLimit(), sourceDataLimits.perSourceByteLimit);
    QCOMPARE(workspaceBudget->aggregateByteLimit(), workspaceLimits.aggregateByteLimit);
    QCOMPARE(workspaceBudget->perOperationByteLimit(), workspaceLimits.perOperationByteLimit);
}

void TestImageDecodeBudgetDefaults::retainedSplitPreservesAggregateUntilEachPhysicalOwnerRetires()
{
    kiriview::ImageDecodeWorkspaceBudget budget(100, 100);
    kiriview::ImageDecodeWorkspaceLease producer
        = kiriview::ImageDecodeWorkspaceDetail::startLease(budget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(producer, 80));

    kiriview::ImageDecodeWorkspaceHold retained = producer.splitRetained(30);
    QVERIFY(retained.isManaged());
    QCOMPARE(retained.reservedByteCount(), qsizetype(30));
    QCOMPARE(producer.reservedByteCount(), qsizetype(50));
    QCOMPARE(budget.reservedByteCount(), qsizetype(80));

    kiriview::ImageDecodeWorkspaceHold retainedAlias = retained;
    retained = {};
    QVERIFY(producer.release(50));
    QCOMPARE(budget.reservedByteCount(), qsizetype(30));

    retainedAlias = {};
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageDecodeBudgetDefaults)

#include "tst_imagedecodebudgetdefaults.moc"
