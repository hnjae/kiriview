// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionRouting : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directVideoRoutesToVideoDocumentWithOriginalSource();
    void publicProjectionRevisionCommitsBeforeScalarSignals();
    void archiveAndDirectoryInputsRouteToImageDocument();
    void directImageAfterVideoRestoresImageDocument();
    void directImageRouteCollectsNavigationSourceFactsOnce();
    void externalSourceAssignmentsProbeExactlyOncePerEntry();
    void archiveScopeNavigationRetainsSingleResolvedSourceSnapshot();
    void kioArchiveImageAfterKioArchiveVideoUsesOriginalImageUrl();
    void directImageDirectMediaNavigationIncludesSiblingVideos();
    void directImageActiveNavigationIgnoresImageDocumentDirectoryPageCandidates();
    void directArchiveEntryImageUsesDirectMediaNavigationWithoutImageDocumentPages();
    void defaultMediaProviderListsLocalDirectImageSiblings();
    void defaultMediaProviderListsLocalDirectVideoSiblings();
    void freshDirectImageReadoutUsesRequestedCursorBeforeDisplayedUrl();
    void directImageDocumentPageCandidateCompletionSurvivesCursorConfirmation();
    void directImageReplacementFailureKeepsTargetMediaCursor();
    void stalePendingDirectImageDocumentPageCandidateCompletionCannotPublishForNewCursor();
    void freshDirectImageFailureKeepsTargetMediaCursor();
    void archiveImageDocumentProjectsActiveNavigationFromPages();
    void imageDocumentPageNavigationChangesEmitActiveNavigationWhenRelevant();
    void activeNavigationNumberDispatchRoutesDirectMedia();
    void activeNavigationNumberDispatchRoutesImageDocumentPages();
};

#include "kiridocumentsession_routing_navigation.inc"
#include "kiridocumentsession_routing_projection.inc"
#include "kiridocumentsession_routing_sources.inc"

QTEST_MAIN(TestKiriDocumentSessionRouting)

#include "tst_kiridocumentsessionrouting.moc"
