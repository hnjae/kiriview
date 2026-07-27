// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentdeletionstate.h"

#include <QObject>
#include <QTest>
#include <limits>

class TestImageDocumentDeletionState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fileDeletionLifecycleRejectsOverlapAndSettlesClaimedOperation();
    void cancelFileDeletionClearsProgressAndRejectsCompletion();
    void operationIdsStayNonZeroAfterWrap();
};

void TestImageDocumentDeletionState::fileDeletionLifecycleRejectsOverlapAndSettlesClaimedOperation()
{
    kiriview::ImageDocumentDeletionState state;

    const kiriview::ImageDocumentDeletionFileOperationStart current = state.startFileDeletion();
    const kiriview::ImageDocumentDeletionFileOperationStart overlapping = state.startFileDeletion();

    QVERIFY(current.accepted);
    QVERIFY(current.operationId != 0);
    QVERIFY(current.inProgressChanged);
    QVERIFY(!overlapping.accepted);
    QCOMPARE(overlapping.operationId, quint64(0));
    QVERIFY(!overlapping.inProgressChanged);
    QVERIFY(state.inProgress());
    QVERIFY(state.acceptsFileDeletion(current.operationId));

    const kiriview::ImageDocumentDeletionFileOperationClaim staleClaim
        = state.claimFileDeletion(current.operationId + 1);
    QVERIFY(!staleClaim.accepted);
    QVERIFY(state.inProgress());

    const kiriview::ImageDocumentDeletionFileOperationClaim currentClaim
        = state.claimFileDeletion(current.operationId);
    QVERIFY(currentClaim.accepted);
    QVERIFY(state.inProgress());
    QVERIFY(!state.acceptsFileDeletion(current.operationId));
    QVERIFY(state.acceptsClaimedFileDeletion(current.operationId));

    const kiriview::ImageDocumentDeletionFileOperationClaim repeatedClaim
        = state.claimFileDeletion(current.operationId);
    QVERIFY(!repeatedClaim.accepted);
    QVERIFY(!state.settleClaimedFileDeletion(current.operationId + 1));
    QVERIFY(state.inProgress());
    QVERIFY(state.settleClaimedFileDeletion(current.operationId));
    QVERIFY(!state.inProgress());
    QVERIFY(!state.acceptsClaimedFileDeletion(current.operationId));
    QVERIFY(!state.settleClaimedFileDeletion(current.operationId));
}

void TestImageDocumentDeletionState::cancelFileDeletionClearsProgressAndRejectsCompletion()
{
    kiriview::ImageDocumentDeletionState state;
    const kiriview::ImageDocumentDeletionFileOperationStart operation = state.startFileDeletion();

    QVERIFY(operation.accepted);
    QVERIFY(state.inProgress());
    QVERIFY(state.cancelFileDeletion());
    QVERIFY(!state.inProgress());

    const kiriview::ImageDocumentDeletionFileOperationClaim claim
        = state.claimFileDeletion(operation.operationId);
    QVERIFY(!claim.accepted);
    QVERIFY(!state.acceptsClaimedFileDeletion(operation.operationId));
    QVERIFY(!state.settleClaimedFileDeletion(operation.operationId));
    QVERIFY(!state.cancelFileDeletion());
}

void TestImageDocumentDeletionState::operationIdsStayNonZeroAfterWrap()
{
    kiriview::ImageDocumentDeletionState state(std::numeric_limits<quint64>::max());

    const kiriview::ImageDocumentDeletionFileOperationStart fileDeletion
        = state.startFileDeletion();

    QVERIFY(fileDeletion.accepted);
    QCOMPARE(fileDeletion.operationId, quint64(1));
}

QTEST_GUILESS_MAIN(TestImageDocumentDeletionState)

#include "tst_imagedocumentdeletionstate.moc"
