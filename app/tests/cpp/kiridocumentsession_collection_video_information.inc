// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionCollectionVideo::
    playableCollectionVideoMediaInformationUsesCollectionEntryWithoutMetadata()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/info-video.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("chapter/01.png"));
    const QUrl videoPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("chapter/clip.mp4"));
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory
        = mediaEntrySourceFactoryForCandidates(
            { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
                kiriview::TestSupport::videoCandidate(videoPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, {}, {}, {},
            std::move(mediaEntrySourceFactory));

    session->setSourceUrl(archiveUrl);
    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);

    session->openActiveNavigationAtNumber(2);

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), videoPage);
    QCOMPARE(session->videoDocument()->sourceUrl(), videoPage);
    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(mediaInformation->available());
    QCOMPARE(session->mediaInformationSnapshot().targetUrl, videoPage);
    QCOMPARE(mediaInformation->title(), QStringLiteral("clip.mp4"));
    QCOMPARE(mediaInformation->summary(), QStringLiteral("Video"));
    QCOMPARE(mediaInformation->mediaSectionTitle(), QStringLiteral("Video"));
    QVERIFY(mediaInformation->canCopyFilePath());
    QVERIFY(mediaInformation->canOpenContainingFolder());
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Type")),
        QStringLiteral("Video"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Path")),
        QStringLiteral("chapter/clip.mp4"));
    QCOMPARE(mediaInformation->mediaRows()->rowCount(), 0);
    QCOMPARE(mediaInformation->cameraRows()->rowCount(), 0);
    QCOMPARE(mediaInformation->advancedRows()->rowCount(), 0);
}
