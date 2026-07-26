// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionCollectionVideo::playableDirectoryCollectionVideoUsesOpenedCollectionNavigation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl directoryUrl = localUrl(directory.path());
    const std::optional<kiriview::OpenedCollectionScopeLocation> directoryCollection
        = kiriview::openedCollectionScopeLocationForResolvedExternalSource(
            kiriview::NavigationSourceResolver().resolveExternalSource(directoryUrl));
    QVERIFY(directoryCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        directoryCollection->rootUrl(), QStringLiteral("chapter/01.png"));
    const QUrl videoPage = kiriview::TestSupport::archivePageUrl(
        directoryCollection->rootUrl(), QStringLiteral("chapter/clip.mp4"));
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory
        = mediaEntrySourceFactoryForCandidates(
            { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
                kiriview::TestSupport::videoCandidate(videoPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, {}, {}, {},
            std::move(mediaEntrySourceFactory));

    session->setSourceUrl(directoryUrl);
    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);

    session->openActiveNavigationAtNumber(2);

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), videoPage);
    QCOMPARE(session->videoDocument()->sourceUrl(), videoPage);
    QVERIFY(!session->activeImageUnsupportedOpenedCollectionVideo());
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::ImageDocumentPageNavigationBoundary);
    QVERIFY(!session->directMediaNavigationBoundaryActive());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(
        session->windowTitleSubject(), QStringLiteral("%1 – 2/2").arg(directoryUrl.fileName()));
}
