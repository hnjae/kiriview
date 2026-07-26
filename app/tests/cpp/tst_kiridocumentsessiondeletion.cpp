// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionDeletion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nonMediaImageDeletionProgressIsMirroredThroughSessionState();
    void directMediaDeletionInProgressDisablesActiveNavigationDispatch();
    void directImageDeletionCanOpenVideoFallback();
    void pendingDirectImageReplacementDoesNotExposeDisplayedDeletion();
    void pendingDirectMediaDeletionCandidateLoadIsCanceledBySourceChange();
    void videoDeletionUsesOriginalUrlAndOpensMediaFallback();
    void canceledVideoDeletionKeepsCurrentVideo();
    void failedVideoDeletionPublishesErrorWithProgressCompletion();
    void staleVideoDeletionCompletionAfterSourceChangeIsIgnored();
};

#include "kiridocumentsession_deletion.inc"
#include "kiridocumentsession_deletion_video.inc"

QTEST_MAIN(TestKiriDocumentSessionDeletion)

#include "tst_kiridocumentsessiondeletion.moc"
