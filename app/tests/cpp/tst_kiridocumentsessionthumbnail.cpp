// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionThumbnail : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directMediaThumbnailModelTracksSiblingCandidates();
    void directMediaThumbnailModelStaysEmptyUntilCandidatesAreKnown();
    void activeNavigationThumbnailModelExposesSourceNeutralResultRoles();
    void activeNavigationThumbnailDemandSurfaceValidatesIdentityAndGeneration();
    void activeNavigationThumbnailDemandProjectsPendingAndUnsupportedResults();
    void directImageThumbnailDemandProjectsReadyCacheHitSource();
    void directImageThumbnailDemandProjectsReadyGeneratedSource();
    void directImageThumbnailDemandKeepsFallbackForFailedLookup();
    void directImageThumbnailDemandKeepsFallbackForFailedGeneration();
    void archiveCollectionThumbnailModelUsesPageCandidateNames();
};

#include "kiridocumentsession_thumbnail_collection.inc"
#include "kiridocumentsession_thumbnail_direct_media.inc"

QTEST_MAIN(TestKiriDocumentSessionThumbnail)

#include "tst_kiridocumentsessionthumbnail.moc"
