// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionDeletion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nonMediaImageDeletionProgressIsMirroredThroughSessionState();
    void nonMediaImageDeletionFailureUsesSettledSessionEvent();
    void directMediaDeletionInProgressDisablesActiveNavigationDispatch();
    void directImageDeletionCanOpenVideoFallback();
    void directImageDeletionWithoutFallbackPublishesCoherentCompletion();
    void pendingDirectImageReplacementDoesNotExposeDisplayedDeletion();
    void directMediaDeletionSupersedesPendingNavigation();
    void pendingDirectMediaDeletionCandidateLoadIsCanceledBySourceChange();
    void sourceChangeDuringDirectMediaDeletionStartCannotRetargetDestructiveRequest();
    void reentrantSourceSelectionDuringDeletionCancellationKeepsLatestSelection();
    void deletionCancellationCallbackCanDestroySession();
    void deletionCancellationReentryCanStartLatestDeletion();
    void videoDeletionUsesOriginalUrlAndOpensMediaFallback();
    void reentrantSourceSelectionDuringSuccessfulVideoDeletionKeepsLatestSelection();
    void stalePendingNavigationCompletionCannotSupersedeDeletionFallback();
    void canceledVideoDeletionKeepsCurrentVideo();
    void failedVideoDeletionPublishesErrorWithProgressCompletion();
    void failedVideoDeletionCompletionAllowsImmediateRetryAndUsesGenericMessage();
    void staleVideoDeletionCompletionAfterSourceChangeIsIgnored();
};

#include "kiridocumentsession_deletion.inc"
#include "kiridocumentsession_deletion_video.inc"

QTEST_MAIN(TestKiriDocumentSessionDeletion)

#include "tst_kiridocumentsessiondeletion.moc"
