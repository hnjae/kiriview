// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionCollectionVideo::unsupportedOpenedCollectionVideoProjectsImagePlaceholderNavigation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/unsupported-video.cb7"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl videoPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("02.mp4"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(archiveCollection->rootUrl(),
        { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
            kiriview::TestSupport::videoCandidate(videoPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());

    session->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);

    const std::size_t loadCountBeforeVideo = dataLoader.loadCount();
    session->openActiveNavigationAtNumber(2);

    QTRY_COMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), videoPage);
    QCOMPARE(session->imageDocument()->sourceUrl(), videoPage);
    QCOMPARE(session->imageDocument()->displayedUrl(), videoPage);
    QVERIFY(session->activeImageUnsupportedOpenedCollectionVideo());
    QVERIFY(!session->activeImageReady());
    QVERIFY(session->activeImageOpenedCollectionScopeActive());
    QVERIFY(session->imageDocument()->twoPageModeAvailable());
    QVERIFY(session->imageDocument()->rightToLeftReadingAvailable());
    session->imageDocument()->requestToggleTwoPageMode();
    session->imageDocument()->requestToggleRightToLeftReading();
    QVERIFY(session->imageDocument()->twoPageModeEnabled());
    QVERIFY(session->imageDocument()->rightToLeftReadingEnabled());
    QVERIFY(session->activeImageRightToLeftReadingActive());
    QVERIFY(!session->activeVideoReady());
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
    QCOMPARE(dataLoader.loadCount(), loadCountBeforeVideo);
}
