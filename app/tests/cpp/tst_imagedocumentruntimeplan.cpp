// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentruntimeplan.h"

#include "image_document_plan_test_support.h"

#include <QObject>
#include <QTest>

namespace {
using kiriview::ImageDocumentRuntimePlan;
using kiriview::TestSupport::hasOperationTypes;
using kiriview::TestSupport::operationAt;
using kiriview::TestSupport::operationTypes;
}

class TestImageDocumentRuntimePlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void clearImagePlansOrderedRuntimeOperations();
    void clearDeletedImagePlansDeletionClearAndEmptySourceCompletion();
    void shutdownPlansOrderedRuntimeOperations();
};

void TestImageDocumentRuntimePlan::clearImagePlansOrderedRuntimeOperations()
{
    const ImageDocumentRuntimePlan plan = kiriview::imageDocumentClearImagePlan();

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::ClearMediaEntrySourceOperation, kiriview::ClearPredecodeOperation,
            kiriview::ClearSecondaryPageOperation, kiriview::CancelPageNavigationUpdateOperation,
            kiriview::ClearPresentationImageOperation, kiriview::ClearPageNavigationOperation,
            kiriview::NotifyRightToLeftReadingChangedOperation>()));
}

void TestImageDocumentRuntimePlan::clearDeletedImagePlansDeletionClearAndEmptySourceCompletion()
{
    const ImageDocumentRuntimePlan plan = kiriview::imageDocumentClearDeletedImagePlan();

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::ClearMediaEntrySourceOperation,
            kiriview::CancelAllNavigationOperation, kiriview::CancelPredecodeOperation,
            kiriview::CancelOpenOperation, kiriview::ClearSecondaryPageOperation,
            kiriview::SelectImageTargetOperation, kiriview::SetErrorStringOperation,
            kiriview::FinishEmptySourceLoadOperation>()));
    QVERIFY(operationAt<kiriview::SelectImageTargetOperation>(plan, 5).target.url.isEmpty());
    QVERIFY(operationAt<kiriview::SetErrorStringOperation>(plan, 6).errorString.isEmpty());
}

void TestImageDocumentRuntimePlan::shutdownPlansOrderedRuntimeOperations()
{
    const ImageDocumentRuntimePlan plan = kiriview::imageDocumentShutdownPlan();

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelFileDeletionOperation, kiriview::ShutdownSpreadOperation,
            kiriview::CancelPredecodeOperation, kiriview::CancelAllNavigationOperation,
            kiriview::CancelOpenOperation, kiriview::ClearMediaEntrySourceOperation>()));
}

QTEST_GUILESS_MAIN(TestImageDocumentRuntimePlan)

#include "tst_imagedocumentruntimeplan.moc"
