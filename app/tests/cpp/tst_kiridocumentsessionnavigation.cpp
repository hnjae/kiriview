// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionNavigation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptySessionProjectsUnavailableActiveNavigation();
    void activeNavigationRequestReportsDispatchAndBoundaryResults();
    void activeNavigationAdjacentRequestsSetRevealIntent();
    void activeNavigationLargeJumpRequestsSetRevealIntent();
    void activeNavigationThumbnailDispatchSetsRevealIntent();
    void sourceOpenReplacesStaleRevealIntent();
    void unavailableActiveNavigationRequestsClearRevealDirection();
    void activeNavigationBoundaryTextFollowsSessionSource();
    void boundaryTextRequestSurvivesSynchronousSessionDestruction();
    void activeNavigationNumberDispatchIgnoresUnknownNavigation();
    void activeNavigationClearsWhenSwitchingFromKnownDirectMedia();
    void activeNavigationAvailabilityUsesSameSnapshotAsCurrentAndCount();
    void activeNavigationBoundaryScopeFollowsSessionSource();
    void routeProjectionReentrantNavigationPreservesNewerRequest();
    void siblingArchiveNavigationContinuesPastEmptyArchive_data();
    void siblingArchiveNavigationContinuesPastEmptyArchive();
};

#include "kiridocumentsession_navigation.inc"
#include "kiridocumentsession_navigation_empty.inc"

QTEST_MAIN(TestKiriDocumentSessionNavigation)

#include "tst_kiridocumentsessionnavigation.moc"
