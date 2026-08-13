// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionNavigation::activeNavigationRequestReportsDispatchAndBoundaryResults()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/request-results.cbz"));
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
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->atKnownFirstActiveNavigation());

    QCOMPARE(session->requestPreviousActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::FirstActiveNavigationBoundary);

    QCOMPARE(session->requestNextActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::ActiveNavigationRequestDispatched);
    QTRY_COMPARE(dataLoader.backLoad().url, secondPage);
    dataLoader.finishBackLoad(QByteArrayLiteral("second"));
    QTRY_COMPARE(session->activeNavigationCurrentNumber(), 2);
    QVERIFY(session->atKnownLastActiveNavigation());

    QCOMPARE(session->requestNextActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::LastActiveNavigationBoundary);
}

void TestKiriDocumentSessionNavigation::activeNavigationAdjacentRequestsSetRevealIntent()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(first);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LoadOrOpen);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);

    session->requestNextActiveNavigation();

    QCOMPARE(session->sourceUrl(), second);
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::AdjacentNavigation);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::Next);

    session->openPreviousActiveNavigation();

    QCOMPARE(session->sourceUrl(), first);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::AdjacentNavigation);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::Previous);

    QCOMPARE(session->requestPreviousActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::FirstActiveNavigationBoundary);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::None);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);
}

void TestKiriDocumentSessionNavigation::activeNavigationLargeJumpRequestsSetRevealIntent()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl third = localUrl(QStringLiteral("/media/03.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second),
            directMediaNavigationCandidate(third) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(second);
    QVERIFY(session->activeNavigationKnown());

    session->openFirstActiveNavigation();

    QCOMPARE(session->sourceUrl(), first);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LargeJump);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);

    session->openLastActiveNavigation();

    QCOMPARE(session->sourceUrl(), third);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LargeJump);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);

    session->openActiveNavigationAtNumber(2);

    QCOMPARE(session->sourceUrl(), second);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LargeJump);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);
}

void TestKiriDocumentSessionNavigation::activeNavigationThumbnailDispatchSetsRevealIntent()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(first);
    QVERIFY(session->activeNavigationKnown());

    session->openActiveNavigationThumbnailAtNumber(2);

    QCOMPARE(session->sourceUrl(), second);
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::ThumbnailActivation);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);
}

void TestKiriDocumentSessionNavigation::sourceOpenReplacesStaleRevealIntent()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl other = localUrl(QStringLiteral("/other/01.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second) });
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/other/")), { directMediaNavigationCandidate(other) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(first);
    QVERIFY(session->activeNavigationKnown());

    session->requestNextActiveNavigation();
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::AdjacentNavigation);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::Next);

    session->setSourceUrl(other);

    QCOMPARE(session->sourceUrl(), other);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LoadOrOpen);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);

    session->openActiveNavigationAtNumber(42);

    QCOMPARE(session->sourceUrl(), other);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::None);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);
}

void TestKiriDocumentSessionNavigation::unavailableActiveNavigationRequestsClearRevealDirection()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    QCOMPARE(session->requestNextActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::None);
    QCOMPARE(session->activeNavigationRevealDirection(),
        KiriDocumentSession::ActiveNavigationRevealDirection::None);
}

void TestKiriDocumentSessionNavigation::activeNavigationBoundaryTextFollowsSessionSource()
{
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/request-boundary-text.cbz"));
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
    FakeDirectMediaNavigationCandidateProvider unusedDirectMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> imageSession
        = createSessionWithProvider(unusedDirectMediaNavigationProvider.provider(), nullptr,
            &dataLoader, imageDocumentPageCandidates.provider());

    imageSession->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(imageSession->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QVERIFY(imageSession->atKnownFirstActiveNavigation());

    QCOMPARE(
        imageSession->requestPreviousActiveNavigationBoundaryText(), QStringLiteral("First image"));
    QCOMPARE(imageSession->requestNextActiveNavigationBoundaryText(), QString());

    QTRY_COMPARE(dataLoader.backLoad().url, secondPage);
    dataLoader.finishBackLoad(QByteArrayLiteral("second"));
    QTRY_COMPARE(imageSession->activeNavigationCurrentNumber(), 2);
    QVERIFY(imageSession->atKnownLastActiveNavigation());

    QCOMPARE(imageSession->requestNextActiveNavigationBoundaryText(), QStringLiteral("Last image"));
}

void TestKiriDocumentSessionNavigation::boundaryTextRequestSurvivesSynchronousSessionDestruction()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(first) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    QObject observer;

    session->setSourceUrl(first);
    QCOMPARE(session->activeNavigationRevealIntent(),
        KiriDocumentSession::ActiveNavigationRevealIntent::LoadOrOpen);

    connect(session.get(), &KiriDocumentSession::publicProjectionRevisionChanged, &observer,
        [&session]() { session.reset(); });

    KiriDocumentSession* const rawSession = session.get();
    const QString boundaryText = rawSession->requestPreviousActiveNavigationBoundaryText();

    QVERIFY(!session);
    QVERIFY(!boundaryText.isEmpty());
}

void TestKiriDocumentSessionNavigation::activeNavigationNumberDispatchIgnoresUnknownNavigation()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider());

    session->setSourceUrl(first);

    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(!session->activeNavigationKnown());
    QVERIFY(!session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 0);
    QCOMPARE(session->activeNavigationCount(), 0);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QCOMPARE(session->requestPreviousActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult);
    QCOMPARE(session->requestNextActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult);

    session->openActiveNavigationAtNumber(2);

    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QCOMPARE(session->sourceUrl(), first);

    directMediaNavigationProvider.finishLoad(
        0, { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second) });
    QVERIFY(session->activeNavigationKnown());
    session->openActiveNavigationAtNumber(2);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));
    directMediaNavigationProvider.finishLoad(
        1, { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second) });
    QCOMPARE(session->sourceUrl(), second);
}

void TestKiriDocumentSessionNavigation::activeNavigationClearsWhenSwitchingFromKnownDirectMedia()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/clear.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl page = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(
        archiveCollection->rootUrl(), { kiriview::TestSupport::imageDocumentPageCandidate(page) });
    const QUrl clip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextClip = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl lastClip = localUrl(QStringLiteral("/media/03.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(nextClip),
            directMediaNavigationCandidate(lastClip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());

    session->setSourceUrl(nextClip);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("02.mp4"));

    session->setSourceUrl(archiveUrl);

    QVERIFY(session->activeNavigationCount() != 3);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("page"));
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 1);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("clear.cbz – 1/1"));

    session->setSourceUrl(nextClip);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);

    session->setSourceUrl(QUrl());

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Empty);
    QCOMPARE(session->windowTitleSubject(), QString());
    compareUnavailableActiveNavigation(*session);
}

void TestKiriDocumentSessionNavigation::
    activeNavigationAvailabilityUsesSameSnapshotAsCurrentAndCount()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl third = localUrl(QStringLiteral("/media/03.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(first), directMediaNavigationCandidate(second),
            directMediaNavigationCandidate(third) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(second);

    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());

    session->openLastActiveNavigation();

    QCOMPARE(session->sourceUrl(), third);
    QCOMPARE(session->activeNavigationCurrentNumber(), 3);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(session->atKnownLastActiveNavigation());

    session->openFirstActiveNavigation();

    QCOMPARE(session->sourceUrl(), first);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
    QVERIFY(session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionNavigation::activeNavigationBoundaryScopeFollowsSessionSource()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/boundary.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl page = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("01.png"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(
        archiveCollection->rootUrl(), { kiriview::TestSupport::imageDocumentPageCandidate(page) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());

    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::NoNavigationBoundary);

    session->setSourceUrl(clip);

    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary);

    session->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("page"));
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_VERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::ImageDocumentPageNavigationBoundary);
}

void TestKiriDocumentSessionNavigation::routeProjectionReentrantNavigationPreservesNewerRequest()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl first = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl second = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl third = localUrl(QStringLiteral("/media/03.mp4"));
    const std::vector<kiriview::DirectMediaNavigationCandidate> candidates {
        directMediaNavigationCandidate(first),
        directMediaNavigationCandidate(second),
        directMediaNavigationCandidate(third),
    };
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider());

    session->setSourceUrl(first);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    directMediaNavigationProvider.finishLoad(0, candidates);
    QVERIFY(session->activeNavigationKnown());

    session->openNextActiveNavigation();
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));

    bool submittedNewerNavigation = false;
    connect(
        session.get(), &KiriDocumentSession::publicProjectionRevisionChanged, session.get(), [&]() {
            if (submittedNewerNavigation || session->sourceUrl() != second) {
                return;
            }
            submittedNewerNavigation = true;
            session->openNextActiveNavigation();
        });

    directMediaNavigationProvider.finishLoad(1, candidates);

    QVERIFY(submittedNewerNavigation);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(3));
    QVERIFY(!directMediaNavigationProvider.loadAt(2).canceled);

    directMediaNavigationProvider.finishLoad(2, candidates);

    QCOMPARE(session->sourceUrl(), third);
}

void TestKiriDocumentSessionNavigation::siblingArchiveNavigationContinuesPastEmptyArchive_data()
{
    QTest::addColumn<bool>("navigateForward");

    QTest::newRow("next") << true;
    QTest::newRow("previous") << false;
}

void TestKiriDocumentSessionNavigation::siblingArchiveNavigationContinuesPastEmptyArchive()
{
    QFETCH(bool, navigateForward);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider pageCandidateProvider;
    const QUrl booksUrl = localUrl(QStringLiteral("/books/"));
    const QUrl firstArchiveUrl = localUrl(QStringLiteral("/books/a.cbz"));
    const QUrl emptyArchiveUrl = localUrl(QStringLiteral("/books/b.cbz"));
    const QUrl lastArchiveUrl = localUrl(QStringLiteral("/books/c.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> firstArchiveScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(firstArchiveUrl, {}));
    const std::optional<kiriview::OpenedCollectionScopeLocation> emptyArchiveScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(emptyArchiveUrl, {}));
    const std::optional<kiriview::OpenedCollectionScopeLocation> lastArchiveScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(lastArchiveUrl, {}));
    QVERIFY(firstArchiveScope.has_value());
    QVERIFY(emptyArchiveScope.has_value());
    QVERIFY(lastArchiveScope.has_value());

    const QUrl firstPageUrl = kiriview::TestSupport::archivePageUrl(
        firstArchiveScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl lastPageUrl = kiriview::TestSupport::archivePageUrl(
        lastArchiveScope->rootUrl(), QStringLiteral("01.png"));
    pageCandidateProvider.setContainerCandidates(booksUrl,
        { kiriview::TestSupport::comicBookContainerCandidate(firstArchiveUrl),
            kiriview::TestSupport::comicBookContainerCandidate(emptyArchiveUrl),
            kiriview::TestSupport::comicBookContainerCandidate(lastArchiveUrl) });

    std::map<QString, std::vector<kiriview::ImageDocumentPageCandidate>> candidatesByScope {
        { keyForUrl(firstArchiveScope->rootUrl()),
            { kiriview::TestSupport::imageDocumentPageCandidate(firstPageUrl) } },
        { keyForUrl(emptyArchiveScope->rootUrl()), {} },
        { keyForUrl(lastArchiveScope->rootUrl()),
            { kiriview::TestSupport::imageDocumentPageCandidate(lastPageUrl) } },
    };
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory
        = [candidatesByScope = std::move(candidatesByScope)](
              const kiriview::OpenedCollectionScopeLocation& scope,
              const kiriview::MediaEntrySourceOpenContext&)
        -> kiriview::MediaEntrySourceOpenResult {
        const auto candidates = candidatesByScope.find(keyForUrl(scope.rootUrl()));
        if (candidates == candidatesByScope.cend()) {
            return std::unexpected(kiriview::MediaEntrySourceError {
                kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                kiriview::MediaEntrySourceBackendKind::Unknown,
                kiriview::MediaEntrySourceOperation::OpenCollection,
                scope.fileUrl(),
                {},
                QStringLiteral("missing fake collection scope"),
            });
        }
        return kiriview::MediaEntrySourcePtr(
            std::make_shared<FakeOpenedCollectionMediaEntrySource>(candidates->second));
    };
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr,
            pageCandidateProvider.provider(), kiriview::TestSupport::staticImageDataDecoder(), {},
            {}, {}, {}, std::move(mediaEntrySourceFactory));

    const QUrl initialArchiveUrl = navigateForward ? firstArchiveUrl : lastArchiveUrl;
    const kiriview::OpenedCollectionScopeLocation& initialArchiveScope
        = navigateForward ? *firstArchiveScope : *lastArchiveScope;
    const QUrl initialPageUrl = navigateForward ? firstPageUrl : lastPageUrl;
    const kiriview::OpenedCollectionScopeLocation& finalArchiveScope
        = navigateForward ? *lastArchiveScope : *firstArchiveScope;
    const QUrl finalPageUrl = navigateForward ? lastPageUrl : firstPageUrl;
    const auto navigate = [&]() {
        if (navigateForward) {
            session->imageDocument()->openNextContainer();
        } else {
            session->imageDocument()->openPreviousContainer();
        }
    };

    session->setSourceUrl(initialArchiveUrl);
    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    QCOMPARE(session->imageDocument()->displayedUrl(), initialPageUrl);
    QCOMPARE(session->imageDocument()->displayedOpenedCollectionScope(), initialArchiveScope);

    navigate();

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Error);
    QCOMPARE(session->imageDocument()->sourceUrl(), emptyArchiveUrl);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QVERIFY(!session->imageDocument()->errorString().isEmpty());

    navigate();

    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    QCOMPARE(session->imageDocument()->displayedUrl(), finalPageUrl);
    QCOMPARE(session->imageDocument()->displayedOpenedCollectionScope(), finalArchiveScope);
    QVERIFY(session->imageDocument()->errorString().isEmpty());
}
