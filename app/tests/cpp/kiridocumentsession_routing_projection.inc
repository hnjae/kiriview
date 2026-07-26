// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionRouting::defaultMediaProviderListsLocalDirectImageSiblings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstImagePath = directory.filePath(QStringLiteral("01.png"));
    const QString videoPath = directory.filePath(QStringLiteral("02.mp4"));
    const QString currentImagePath = directory.filePath(QStringLiteral("03.png"));
    QVERIFY(writeTestImage(firstImagePath));
    QVERIFY(writeEmptyFile(videoPath));
    QVERIFY(writeTestImage(currentImagePath));

    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(kiriview::DirectMediaNavigationCandidateProvider {});

    session->setSourceUrl(localUrl(currentImagePath));

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QTRY_VERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 3);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::defaultMediaProviderListsLocalDirectVideoSiblings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstImagePath = directory.filePath(QStringLiteral("01.png"));
    const QString currentVideoPath = directory.filePath(QStringLiteral("02.mp4"));
    const QString nextVideoPath = directory.filePath(QStringLiteral("03.mov"));
    QVERIFY(writeTestImage(firstImagePath));
    QVERIFY(writeEmptyFile(currentVideoPath));
    QVERIFY(writeEmptyFile(nextVideoPath));

    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(kiriview::DirectMediaNavigationCandidateProvider {});

    session->setSourceUrl(localUrl(currentVideoPath));

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), localUrl(currentVideoPath));
    QVERIFY(session->activeNavigationAvailable());
    QTRY_VERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::freshDirectImageReadoutUsesRequestedCursorBeforeDisplayedUrl()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, nullptr, &imageDataLoader);

    session->setSourceUrl(imageUrl);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QVERIFY(session->activeNavigationAvailable());
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("01.png"));
    QVERIFY(!session->activeImageReady());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
}

void TestKiriDocumentSessionRouting::directImageDocumentPageCandidateCompletionSurvivesCursorConfirmation()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl siblingUrl = localUrl(QStringLiteral("/media/02.mp4"));
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), nullptr, &imageDataLoader);

    session->setSourceUrl(imageUrl);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QCOMPARE(
        directMediaNavigationProvider.loadAt(0).parentUrl, localUrl(QStringLiteral("/media/")));
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(!session->activeNavigationKnown());
    QVERIFY(!session->activeNavigationEditable());

    imageDataLoader.finishBackLoad(QByteArrayLiteral("image"));

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->imageDocument()->displayedUrl(), imageUrl);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QVERIFY(!directMediaNavigationProvider.loadAt(0).canceled);
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(!session->activeNavigationKnown());
    QVERIFY(!session->activeNavigationEditable());

    directMediaNavigationProvider.finishLoad(0,
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(siblingUrl) });

    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(session->canOpenNextActiveNavigation());
}

void TestKiriDocumentSessionRouting::directImageReplacementFailureKeepsTargetMediaCursor()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl firstImage = localUrl(QStringLiteral("/media/01.png"));
    const QUrl secondImage = localUrl(QStringLiteral("/media/02.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstImage),
            directMediaNavigationCandidate(secondImage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, nullptr, &imageDataLoader);

    session->setSourceUrl(firstImage);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    imageDataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->imageDocument()->displayedUrl(), firstImage);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());

    session->setSourceUrl(secondImage);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(2));
    QCOMPARE(session->imageDocument()->displayedUrl(), firstImage);
    QCOMPARE(session->sourceUrl(), secondImage);
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("02.png"));
    QVERIFY(!session->activeImageReady());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);

    imageDataLoader.failBackLoad(QStringLiteral("replacement failed"));

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Error);
    QCOMPARE(session->sourceUrl(), secondImage);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("02.png"));
    QVERIFY(!session->activeImageReady());
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(session->atKnownLastActiveNavigation());
}

void TestKiriDocumentSessionRouting::
    stalePendingDirectImageDocumentPageCandidateCompletionCannotPublishForNewCursor()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), nullptr, &imageDataLoader);

    const QUrl firstImage = localUrl(QStringLiteral("/first/01.png"));
    const QUrl staleSibling = localUrl(QStringLiteral("/first/02.mp4"));
    const QUrl secondImage = localUrl(QStringLiteral("/second/01.png"));

    session->setSourceUrl(firstImage);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QCOMPARE(
        directMediaNavigationProvider.loadAt(0).parentUrl, localUrl(QStringLiteral("/first/")));

    session->setSourceUrl(secondImage);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));
    QCOMPARE(
        directMediaNavigationProvider.loadAt(1).parentUrl, localUrl(QStringLiteral("/second/")));
    QVERIFY(directMediaNavigationProvider.loadAt(0).canceled);
    QVERIFY(!session->activeNavigationKnown());

    directMediaNavigationProvider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(firstImage),
            directMediaNavigationCandidate(staleSibling) });

    QVERIFY(!session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCount(), 0);

    directMediaNavigationProvider.finishLoad(1, { directMediaNavigationCandidate(secondImage) });

    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 1);
}

void TestKiriDocumentSessionRouting::freshDirectImageFailureKeepsTargetMediaCursor()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl siblingUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(siblingUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, nullptr, &imageDataLoader);

    session->setSourceUrl(imageUrl);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);

    imageDataLoader.failBackLoad(QStringLiteral("initial load failed"));

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Error);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
}

void TestKiriDocumentSessionRouting::archiveImageDocumentProjectsActiveNavigationFromPages()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
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

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::ImageDocumentPageNavigationBoundary);
    QVERIFY(session->activeNavigationAvailable());
    QTRY_COMPARE(session->imageDocument()->currentPageNumber(), 1);
    QTRY_COMPARE(session->imageDocument()->currentLastPageNumber(), 1);
    QTRY_COMPARE(session->imageDocument()->pageCount(), 2);
    QTRY_VERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(session->windowTitleSubject(), QStringLiteral("book.cbz – 1/2"));
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());
    QVERIFY(session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());
}
