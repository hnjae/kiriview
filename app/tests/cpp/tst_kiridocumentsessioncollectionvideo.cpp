// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionCollectionVideo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void playableCollectionVideoMediaInformationUsesCollectionEntryWithoutMetadata();
    void playableDirectoryCollectionVideoUsesOpenedCollectionNavigation();
    void unsupportedOpenedCollectionVideoProjectsImagePlaceholderNavigation();
    void playableCollectionVideoDeletionTargetsOpenedCollectionContainerAndClearsPlayback();
};

#include "kiridocumentsession_collection_video_deletion.inc"
#include "kiridocumentsession_collection_video_information.inc"
#include "kiridocumentsession_collection_video_placeholder.inc"
#include "kiridocumentsession_collection_video_routing.inc"

QTEST_MAIN(TestKiriDocumentSessionCollectionVideo)

#include "tst_kiridocumentsessioncollectionvideo.moc"
