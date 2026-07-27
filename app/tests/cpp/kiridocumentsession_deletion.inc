// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionDeletion::nonMediaImageDeletionProgressIsMirroredThroughSessionState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("page.png"));
    QImage image(QSize(2, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    QVERIFY(image.save(imagePath, "PNG"));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl directoryUrl = localUrl(directory.path());
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    QSignalSpy progressSpy(session.get(), &KiriDocumentSession::fileDeletionInProgressChanged);

    session->setSourceUrl(directoryUrl);
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::ImageDocumentPageNavigationBoundary);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, directoryUrl);
    QVERIFY(session->fileDeletionInProgress());
    QCOMPARE(progressSpy.count(), 1);

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(progressSpy.count(), 2);
}

void TestKiriDocumentSessionDeletion::directMediaDeletionInProgressDisablesActiveNavigationDispatch()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl firstClip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl currentClip = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl lastClip = localUrl(QStringLiteral("/media/03.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstClip), directMediaNavigationCandidate(currentClip),
            directMediaNavigationCandidate(lastClip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);

    session->setSourceUrl(currentClip);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(session->displayedFileDeletionAvailable());
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(session->canOpenNextActiveNavigation());

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, currentClip);
    QVERIFY(session->fileDeletionInProgress());
    QVERIFY(!session->displayedFileDeletionAvailable());
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QVERIFY(!session->activeNavigationEditable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 3);
    QVERIFY(!session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());
    QVERIFY(!session->atKnownFirstActiveNavigation());
    QVERIFY(!session->atKnownLastActiveNavigation());
    QCOMPARE(session->requestPreviousActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult);
    QCOMPARE(session->requestNextActiveNavigation(),
        KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult);

    session->openPreviousActiveNavigation();
    session->openNextActiveNavigation();
    session->openFirstActiveNavigation();
    session->openLastActiveNavigation();
    session->openActiveNavigationAtNumber(1);
    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(session->sourceUrl(), currentClip);
    QCOMPARE(session->videoDocument()->sourceUrl(), currentClip);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Canceled);

    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->sourceUrl(), currentClip);
}

void TestKiriDocumentSessionDeletion::directImageDeletionCanOpenVideoFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QImage image(QSize(2, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    QVERIFY(image.save(imagePath, "PNG"));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl imageUrl = localUrl(imagePath);
    const QUrl videoUrl = localUrl(directory.filePath(QStringLiteral("02.mp4")));
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(imageUrl);
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationBoundaryScope(),
        KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, imageUrl);

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(session->sourceUrl(), videoUrl);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), videoUrl);
}

void TestKiriDocumentSessionDeletion::pendingDirectImageReplacementDoesNotExposeDisplayedDeletion()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl firstImage = localUrl(QStringLiteral("/media/01.png"));
    const QUrl secondImage = localUrl(QStringLiteral("/media/02.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstImage),
            directMediaNavigationCandidate(secondImage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider, &imageDataLoader);

    session->setSourceUrl(firstImage);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    imageDataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->imageDocument()->displayedUrl(), firstImage);

    session->setSourceUrl(secondImage);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(2));
    QCOMPARE(session->imageDocument()->displayedUrl(), firstImage);
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QVERIFY(!session->activeImageReady());
    QVERIFY(!session->displayedFileDeletionAvailable());
    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));

    imageDataLoader.failBackLoad(QStringLiteral("replacement failed"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Error);
    QCOMPARE(session->sourceUrl(), secondImage);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QVERIFY(!session->displayedFileDeletionAvailable());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
}

void TestKiriDocumentSessionDeletion::pendingDirectMediaDeletionCandidateLoadIsCanceledBySourceChange()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl firstClip = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl firstFallback = localUrl(QStringLiteral("/first/02.png"));
    const QUrl secondClip = localUrl(QStringLiteral("/second/01.mp4"));
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider);

    session->setSourceUrl(firstClip);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    directMediaNavigationProvider.finishLoad(0,
        { directMediaNavigationCandidate(firstClip),
            directMediaNavigationCandidate(firstFallback) });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));
    QVERIFY(session->fileDeletionInProgress());
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));

    session->setSourceUrl(secondClip);

    QCOMPARE(session->sourceUrl(), secondClip);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(!session->fileDeletionInProgress());
    QVERIFY(directMediaNavigationProvider.loadAt(1).canceled);

    directMediaNavigationProvider.deliverIgnoringCancellation(1,
        { directMediaNavigationCandidate(firstClip),
            directMediaNavigationCandidate(firstFallback) });

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
    QCOMPARE(session->sourceUrl(), secondClip);
    QVERIFY(!session->fileDeletionInProgress());
}

void TestKiriDocumentSessionDeletion::
    reentrantSourceSelectionDuringDeletionCancellationKeepsLatestSelection()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl firstImage = localUrl(QStringLiteral("/media/01.png"));
    const QUrl supersededImage = localUrl(QStringLiteral("/media/02.png"));
    const QUrl latestImage = localUrl(QStringLiteral("/media/03.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstImage),
            directMediaNavigationCandidate(supersededImage),
            directMediaNavigationCandidate(latestImage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider, &imageDataLoader);

    session->setSourceUrl(firstImage);
    imageDataLoader.finishBackLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QVERIFY(session->fileDeletionInProgress());

    bool latestSelectionSubmitted = false;
    connect(session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(),
        [&]() {
            if (session->fileDeletionInProgress() || latestSelectionSubmitted) {
                return;
            }
            latestSelectionSubmitted = true;
            session->setSourceUrl(latestImage);
        });

    session->setSourceUrl(supersededImage);

    QVERIFY(latestSelectionSubmitted);
    QCOMPARE(session->sourceUrl(), latestImage);
    QCOMPARE(session->imageDocument()->sourceUrl(), latestImage);
    QVERIFY(imageDataLoader.hasActiveLoadForUrl(latestImage));
    QVERIFY(!imageDataLoader.hasActiveLoadForUrl(supersededImage));
}
