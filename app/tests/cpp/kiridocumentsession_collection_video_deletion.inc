// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionCollectionVideo::
    playableCollectionVideoDeletionTargetsOpenedCollectionContainerAndClearsPlayback()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/delete-video.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl videoPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("02.mp4"));
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory
        = mediaEntrySourceFactoryForCandidates(
            { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
                kiriview::TestSupport::videoCandidate(videoPage) });
    imageDocumentPageCandidates.setContainerCandidates(localUrl(QStringLiteral("/books/")), {});
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider, nullptr,
        imageDocumentPageCandidates.provider(), kiriview::TestSupport::staticImageDataDecoder(), {},
        {}, {}, {}, std::move(mediaEntrySourceFactory));

    session->setSourceUrl(archiveUrl);
    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    session->openActiveNavigationAtNumber(2);
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), videoPage);
    QVERIFY(session->displayedFileDeletionAvailable());

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, archiveUrl);
    QVERIFY(session->fileDeletionInProgress());

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Empty);
    QCOMPARE(session->sourceUrl(), QUrl());
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
    QVERIFY(!session->activeVideoReady());
}
