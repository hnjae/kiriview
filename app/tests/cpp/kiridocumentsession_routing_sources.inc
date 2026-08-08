// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionRouting::directVideoRoutesToVideoDocumentWithOriginalSource()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/clip.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(clip);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), clip);
    QCOMPARE(session->videoDocument()->sourceUrl(), clip);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("clip.mp4"));
    QCOMPARE(session->imageDocument()->sourceUrl(), QUrl());
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 1);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(session->atKnownFirstActiveNavigation());
    QVERIFY(session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::removedDirectVideoClearsOldLeafBeforeRoutingFallback()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl previousClip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl removedClip = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl fallbackClip = localUrl(QStringLiteral("/media/03.mp4"));
    const QUrl parentUrl = localUrl(QStringLiteral("/media/"));
    directMediaNavigationProvider.setMedia(parentUrl,
        { directMediaNavigationCandidate(previousClip), directMediaNavigationCandidate(removedClip),
            directMediaNavigationCandidate(fallbackClip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    session->setSourceUrl(removedClip);
    std::vector<QUrl> videoSourceChanges;
    connect(session->videoDocument(), &KiriVideoDocument::sourceUrlChanged, session.get(), [&]() {
        videoSourceChanges.push_back(session->videoDocument()->sourceUrl());
    });

    directMediaNavigationProvider.publishMediaChanges(parentUrl,
        { directMediaNavigationCandidate(previousClip),
            directMediaNavigationCandidate(fallbackClip) });

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), fallbackClip);
    QCOMPARE(session->videoDocument()->sourceUrl(), fallbackClip);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    const auto cleared = std::ranges::find(videoSourceChanges, QUrl());
    QVERIFY(cleared != videoSourceChanges.cend());
    const auto fallback = std::ranges::find(std::next(cleared), videoSourceChanges.cend(), fallbackClip);
    QVERIFY(fallback != videoSourceChanges.cend());
}

void TestKiriDocumentSessionRouting::removedOnlyDirectVideoClearsSession()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl removedClip = localUrl(QStringLiteral("/media/only.mp4"));
    const QUrl parentUrl = localUrl(QStringLiteral("/media/"));
    directMediaNavigationProvider.setMedia(
        parentUrl, { directMediaNavigationCandidate(removedClip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    session->setSourceUrl(removedClip);

    directMediaNavigationProvider.publishMediaChanges(parentUrl, {});

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Empty);
    QCOMPARE(session->sourceUrl(), QUrl());
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
    QVERIFY(!session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCount(), 0);
}

void TestKiriDocumentSessionRouting::directVideoTitleDecodesFileNameExactlyOnce_data()
{
    QTest::addColumn<QUrl>("clip");
    QTest::addColumn<QString>("expectedTitle");

    QTest::newRow("reserved-brackets")
        << localUrl(QStringLiteral("/media/[clip].mp4")) << QStringLiteral("[clip].mp4");
    QTest::newRow("literal-percent-sequence")
        << localUrl(QStringLiteral("/media/%5Bclip%5D.mp4")) << QStringLiteral("%5Bclip%5D.mp4");
}

void TestKiriDocumentSessionRouting::directVideoTitleDecodesFileNameExactlyOnce()
{
    QFETCH(QUrl, clip);
    QFETCH(QString, expectedTitle);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(clip);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), clip);
    QCOMPARE(session->videoDocument()->sourceUrl(), clip);
    QCOMPARE(session->windowTitleSubject(), expectedTitle);
}

void TestKiriDocumentSessionRouting::publicProjectionRevisionCommitsBeforeScalarSignals()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/clip.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    QStringList events;
    const QString committedRevisionEvent = QStringLiteral("revision:%1").arg(clip.toString());

    connect(session.get(), &KiriDocumentSession::publicProjectionRevisionChanged, session.get(),
        [&session, &events, &clip]() {
            events.append(QStringLiteral("revision:%1").arg(session->sourceUrl().toString()));
            if (session->sourceUrl() == clip) {
                QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
                QCOMPARE(session->windowTitleSubject(), QStringLiteral("clip.mp4"));
            }
        });
    connect(session.get(), &KiriDocumentSession::sourceUrlChanged, session.get(),
        [&events]() { events.append(QStringLiteral("source")); });
    connect(session.get(), &KiriDocumentSession::documentKindChanged, session.get(),
        [&events]() { events.append(QStringLiteral("kind")); });

    QCOMPARE(session->publicProjectionRevision(), quint64(0));

    session->setSourceUrl(clip);

    QVERIFY(session->publicProjectionRevision() > 0);
    const int revisionIndex = events.indexOf(committedRevisionEvent);
    const int sourceIndex = events.indexOf(QStringLiteral("source"));
    QVERIFY(revisionIndex >= 0);
    QVERIFY(sourceIndex >= 0);
    QVERIFY(revisionIndex < sourceIndex);
}

void TestKiriDocumentSessionRouting::destroyingSessionDuringPublicSignalStopsFanOut()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/destroy-during-signal.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    QObject observer;
    int sourceSignalCountAfterDestruction = 0;

    connect(session.get(), &KiriDocumentSession::publicProjectionRevisionChanged, &observer,
        [&session, &clip]() {
            if (session && session->sourceUrl() == clip) {
                session.reset();
            }
        });
    connect(session.get(), &KiriDocumentSession::sourceUrlChanged, &observer,
        [&sourceSignalCountAfterDestruction]() { ++sourceSignalCountAfterDestruction; });

    KiriDocumentSession* const rawSession = session.get();
    rawSession->setSourceUrl(clip);

    QVERIFY(!session);
    QCOMPARE(sourceSignalCountAfterDestruction, 0);
}

void TestKiriDocumentSessionRouting::archiveAndDirectoryInputsRouteToImageDocument()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    const QUrl archive = localUrl(QStringLiteral("/books/book.zip"));
    session->setSourceUrl(archive);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->imageDocument()->sourceUrl(), archive);
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());

    const QUrl directory = localUrl(QStringLiteral("/books/"));
    session->setSourceUrl(directory);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->imageDocument()->sourceUrl(), directory);
}

void TestKiriDocumentSessionRouting::directImageAfterVideoRestoresImageDocument()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl image = localUrl(QStringLiteral("/media/02.svg"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(image) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(clip);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), clip);

    session->setSourceUrl(image);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), image);
    QCOMPARE(session->imageDocument()->sourceUrl(), image);
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
}

void TestKiriDocumentSessionRouting::directImageRouteCollectsNavigationSourceFactsOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl imageUrl = localUrl(directory.filePath(QStringLiteral("page.png")));
    int probeCount = 0;
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            {}, kiriview::TestSupport::staticImageDataDecoder(), {}, {}, {}, {}, {},
            [&probeCount, &imageUrl](const QUrl& url) {
                if (url == imageUrl) {
                    ++probeCount;
                }
                return kiriview::NavigationSourceEntryFacts {};
            });

    session->setSourceUrl(imageUrl);
    QCOMPARE(probeCount, 1);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(imageUrl, QByteArrayLiteral("image bytes")));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(probeCount, 1);
}

void TestKiriDocumentSessionRouting::externalSourceAssignmentsProbeExactlyOncePerEntry()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/media/probe.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/probe.mp4"));
    int probeCount = 0;
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, {}, {}, {}, {},
            [&probeCount](const QUrl&) {
                ++probeCount;
                return kiriview::NavigationSourceEntryFacts {};
            });

    session->setSourceUrl(imageUrl);
    QCOMPARE(probeCount, 1);

    session->setSourceUrl(imageUrl);
    QCOMPARE(probeCount, 2);

    session->setSourceUrl(QUrl());
    QCOMPARE(probeCount, 2);

    session->setSourceUrl(imageUrl);
    QCOMPARE(probeCount, 3);

    session->setSourceUrl(videoUrl);
    QCOMPARE(probeCount, 4);
}

void TestKiriDocumentSessionRouting::archiveScopeNavigationRetainsSingleResolvedSourceSnapshot()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/probe-lifetime.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveScope.has_value());
    const QUrl firstPage
        = kiriview::TestSupport::archivePageUrl(archiveScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPage
        = kiriview::TestSupport::archivePageUrl(archiveScope->rootUrl(), QStringLiteral("02.png"));
    pageCandidateProvider.setOpenedCollectionCandidates(archiveScope->rootUrl(),
        { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
            kiriview::TestSupport::imageDocumentPageCandidate(secondPage) });
    int probeCount = 0;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            pageCandidateProvider.provider(), kiriview::TestSupport::staticImageDataDecoder(), {},
            {}, {}, {}, {}, [&probeCount](const QUrl&) {
                ++probeCount;
                return kiriview::NavigationSourceEntryFacts {};
            });

    session->setSourceUrl(archiveUrl);
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(firstPage, QByteArrayLiteral("first")));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(probeCount, 1);

    session->imageDocument()->openImageAtPage(2);
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(secondPage, QByteArrayLiteral("second")));
    QTRY_COMPARE(session->imageDocument()->displayedUrl(), secondPage);
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(probeCount, 1);

    session->imageDocument()->openImageAtPage(1);
    dataLoader.finishNewestActiveLoadForUrl(firstPage, QByteArrayLiteral("first-again"));
    QTRY_COMPARE(session->imageDocument()->displayedUrl(), firstPage);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(probeCount, 1);
}

void TestKiriDocumentSessionRouting::kioArchiveImageAfterKioArchiveVideoUsesOriginalImageUrl()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl parentUrl(QStringLiteral("zip:///books/book.zip!/chapter/"));
    const QUrl videoUrl(QStringLiteral("zip:///books/book.zip!/chapter/clip.mp4"));
    const QUrl imageUrl(QStringLiteral("zip:///books/book.zip!/chapter/page.png"));
    directMediaNavigationProvider.setMedia(parentUrl, { directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader);

    session->setSourceUrl(videoUrl);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), videoUrl);
    QCOMPARE(session->videoDocument()->sourceUrl(), videoUrl);

    directMediaNavigationProvider.setMedia(parentUrl, { directMediaNavigationCandidate(imageUrl) });
    session->setSourceUrl(imageUrl);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), imageUrl);
    QCOMPARE(session->imageDocument()->sourceUrl(), imageUrl);
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.backLoad().url, imageUrl);
    QVERIFY(dataLoader.backLoad().openedCollectionScope.isEmpty());
    QVERIFY(!dataLoader.backLoad().url.isLocalFile());
    QVERIFY(!dataLoader.backLoad().url.toString().contains(QStringLiteral("kio-fuse")));
}

void TestKiriDocumentSessionRouting::directImageDirectMediaNavigationIncludesSiblingVideos()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    const QUrl videoUrl = localUrl(directory.filePath(QStringLiteral("02.mp4")));
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(imageUrl);

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("01.png – 2×2"));
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
    QVERIFY(session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());

    session->openActiveNavigationAtNumber(2);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), videoUrl);
    QCOMPARE(session->videoDocument()->sourceUrl(), videoUrl);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("02.mp4"));
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::
    directImageActiveNavigationIgnoresImageDocumentDirectoryPageCandidates()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl extraImageUrl = localUrl(QStringLiteral("/media/03.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    imageDocumentPageCandidates.setDirectoryImages(localUrl(QStringLiteral("/media/")),
        { kiriview::TestSupport::imageDocumentPageCandidate(imageUrl),
            kiriview::ImageDocumentPageCandidate {
                videoUrl, QStringLiteral("02.mp4"), kiriview::ImageDocumentPageKind::Video },
            kiriview::TestSupport::imageDocumentPageCandidate(extraImageUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr,
            &imageDataLoader, imageDocumentPageCandidates.provider());

    session->setSourceUrl(imageUrl);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    imageDataLoader.finishBackLoad(QByteArrayLiteral("image"));

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(session->imageDocument()->currentPageNumber(), 0);
    QCOMPARE(session->imageDocument()->pageCount(), 0);
}

void TestKiriDocumentSessionRouting::
    directArchiveEntryImageUsesDirectMediaNavigationWithoutImageDocumentPages()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl directoryUrl(QStringLiteral("zip:///path/archive.zip!/chapter/"));
    const QUrl imageUrl(QStringLiteral("zip:///path/archive.zip!/chapter/01.png"));
    const QUrl videoUrl(QStringLiteral("zip:///path/archive.zip!/chapter/02.mp4"));
    directMediaNavigationProvider.setMedia(directoryUrl,
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    imageDocumentPageCandidates.setDirectoryImages(directoryUrl,
        { kiriview::TestSupport::imageDocumentPageCandidate(imageUrl),
            kiriview::ImageDocumentPageCandidate {
                videoUrl, QStringLiteral("02.mp4"), kiriview::ImageDocumentPageKind::Video } });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr,
            &imageDataLoader, imageDocumentPageCandidates.provider());

    session->setSourceUrl(imageUrl);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    imageDataLoader.finishBackLoad(QByteArrayLiteral("image"));

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(session->imageDocument()->currentPageNumber(), 0);
    QCOMPARE(session->imageDocument()->pageCount(), 0);
}
