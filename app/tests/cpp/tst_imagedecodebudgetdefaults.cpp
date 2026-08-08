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

QTEST_GUILESS_MAIN(TestImageDecodeBudgetDefaults)

#include "tst_imagedecodebudgetdefaults.moc"
