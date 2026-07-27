// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionRouting::
    imageDocumentPageNavigationChangesEmitActiveNavigationWhenRelevant()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/signals.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("02.png"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(archiveCollection->rootUrl(),
        { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
            kiriview::TestSupport::imageDocumentPageCandidate(secondPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());
    QSignalSpy activeNavigationSpy(session.get(), &KiriDocumentSession::activeNavigationChanged);

    session->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QTRY_COMPARE(session->activeNavigationCount(), 2);
    const int baselineSignalCount = activeNavigationSpy.count();

    session->imageDocument()->openImageAtPage(2);

    QTRY_COMPARE(session->activeNavigationCurrentNumber(), 2);
    QVERIFY(activeNavigationSpy.count() > baselineSignalCount);
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::activeNavigationNumberDispatchRoutesDirectMedia()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(videoUrl);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);

    session->openActiveNavigationAtNumber(1);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), imageUrl);
    QCOMPARE(session->imageDocument()->sourceUrl(), imageUrl);
}

void TestKiriDocumentSessionRouting::activeNavigationNumberDispatchRoutesImageDocumentPages()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/number-dispatch.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("02.png"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(archiveCollection->rootUrl(),
        { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
            kiriview::TestSupport::imageDocumentPageCandidate(secondPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());

    session->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QSignalSpy leafSnapshotSpy(session->imageDocument(), SIGNAL(documentSessionSnapshotChanged()));
    const int snapshotCountBeforeNavigation = leafSnapshotSpy.count();

    session->openActiveNavigationAtNumber(2);

    QTRY_VERIFY(leafSnapshotSpy.count() > snapshotCountBeforeNavigation);
    QTRY_COMPARE(dataLoader.backLoad().url, secondPage);
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QCOMPARE(session->imageDocument()->displayedOpenedCollectionScope(), *archiveCollection);
    QVERIFY(session->activeImageOpenedCollectionScopeActive());
    QVERIFY(!session->activeImageReady());
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("number-dispatch.cbz – 2/2"));
    dataLoader.finishBackLoad(QByteArrayLiteral("second"));
    QTRY_COMPARE(session->imageDocument()->currentPageNumber(), 2);
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
}
