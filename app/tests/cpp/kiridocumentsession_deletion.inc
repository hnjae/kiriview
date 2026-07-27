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
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler imageWorkerScheduler;
    const QUrl directoryUrl = localUrl(directory.path());
    const QUrl imageUrl = localUrl(imagePath);
    pageCandidateProvider.setOpenedCollectionCandidates(
        kiriview::normalizedDirectoryContainerUrl(directoryUrl),
        { kiriview::TestSupport::imageDocumentPageCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider, &imageDataLoader,
        pageCandidateProvider.provider(), kiriview::TestSupport::staticImageDataDecoder(), {}, {},
        {}, {}, {},
        [directoryUrl](const QUrl& url) {
            return kiriview::NavigationSourceEntryFacts {
                {},
                {},
                url == directoryUrl,
            };
        },
        imageWorkerScheduler.scheduler());
    QSignalSpy progressSpy(session.get(), &KiriDocumentSession::fileDeletionInProgressChanged);

    session->setSourceUrl(directoryUrl);
    QTRY_COMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, imageUrl);
    imageDataLoader.finishBackLoad(QByteArrayLiteral("image"));
    QCOMPARE(imageWorkerScheduler.scheduleCount(), std::size_t(1));
    imageWorkerScheduler.runWork(0);
    imageWorkerScheduler.finish(0);
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QVERIFY(driveTestViewportUntil(*session,
        [&]() { return session->imageDocument()->status() == KiriImageDocument::Status::Ready; }));
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

void TestKiriDocumentSessionDeletion::nonMediaImageDeletionFailureUsesSettledSessionEvent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("page.png"));
    QImage image(QSize(2, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    QVERIFY(image.save(imagePath, "PNG"));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler imageWorkerScheduler;
    const QUrl directoryUrl = localUrl(directory.path());
    const QUrl imageUrl = localUrl(imagePath);
    pageCandidateProvider.setOpenedCollectionCandidates(
        kiriview::normalizedDirectoryContainerUrl(directoryUrl),
        { kiriview::TestSupport::imageDocumentPageCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider, &imageDataLoader,
        pageCandidateProvider.provider(), kiriview::TestSupport::staticImageDataDecoder(), {}, {},
        {}, {}, {},
        [directoryUrl](const QUrl& url) {
            return kiriview::NavigationSourceEntryFacts {
                {},
                {},
                url == directoryUrl,
            };
        },
        imageWorkerScheduler.scheduler());

    session->setSourceUrl(directoryUrl);
    QTRY_COMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, imageUrl);
    imageDataLoader.finishBackLoad(QByteArrayLiteral("image"));
    QCOMPARE(imageWorkerScheduler.scheduleCount(), std::size_t(1));
    imageWorkerScheduler.runWork(0);
    imageWorkerScheduler.finish(0);
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QVERIFY(driveTestViewportUntil(*session,
        [&]() { return session->imageDocument()->status() == KiriImageDocument::Status::Ready; }));

    bool failureNotified = false;
    bool deletionInProgressAtFailure = true;
    QString failureMessage;
    connect(session.get(), &KiriDocumentSession::fileDeletionFailed, session.get(),
        [&](const QString& message) {
            failureNotified = true;
            deletionInProgressAtFailure = session->fileDeletionInProgress();
            failureMessage = message;
            session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
        });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Failed);

    QVERIFY(failureNotified);
    QVERIFY(!deletionInProgressAtFailure);
    QVERIFY(!failureMessage.isEmpty());
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(2));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, directoryUrl);
    QVERIFY(session->fileDeletionInProgress());
}

void TestKiriDocumentSessionDeletion::
    directMediaDeletionInProgressDisablesActiveNavigationDispatch()
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

void TestKiriDocumentSessionDeletion::
    directImageDeletionWithoutFallbackPublishesCoherentCompletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("only.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(imageUrl);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);

    int completionCount = 0;
    QUrl sourceAtCompletion;
    KiriDocumentSession::DocumentKind kindAtCompletion = KiriDocumentSession::DocumentKind::Image;
    QUrl imageSourceAtCompletion;
    QUrl displayedImageAtCompletion;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (session->fileDeletionInProgress()) {
                return;
            }
            ++completionCount;
            sourceAtCompletion = session->sourceUrl();
            kindAtCompletion = session->documentKind();
            imageSourceAtCompletion = session->imageDocument()->sourceUrl();
            displayedImageAtCompletion = session->imageDocument()->displayedUrl();
        });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(completionCount, 1);
    QCOMPARE(sourceAtCompletion, QUrl());
    QCOMPARE(kindAtCompletion, KiriDocumentSession::DocumentKind::Empty);
    QCOMPARE(imageSourceAtCompletion, QUrl());
    QCOMPARE(displayedImageAtCompletion, QUrl());
    QCOMPARE(session->sourceUrl(), QUrl());
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Empty);
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
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
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

void TestKiriDocumentSessionDeletion::directMediaDeletionSupersedesPendingNavigation()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl currentClip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextClip = localUrl(QStringLiteral("/media/02.mp4"));
    const std::vector<kiriview::DirectMediaNavigationCandidate> candidates {
        directMediaNavigationCandidate(currentClip),
        directMediaNavigationCandidate(nextClip),
    };
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider);

    session->setSourceUrl(currentClip);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    directMediaNavigationProvider.finishLoad(0, candidates);
    QVERIFY(session->activeNavigationKnown());

    session->openNextActiveNavigation();
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));
    QVERIFY(!directMediaNavigationProvider.loadAt(1).canceled);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(3));
    QVERIFY(directMediaNavigationProvider.loadAt(1).canceled);
    QVERIFY(!directMediaNavigationProvider.loadAt(2).canceled);
    QVERIFY(session->fileDeletionInProgress());

    directMediaNavigationProvider.deliverIgnoringCancellation(1, candidates);

    QCOMPARE(session->sourceUrl(), currentClip);
    QCOMPARE(session->videoDocument()->sourceUrl(), currentClip);
    QVERIFY(session->fileDeletionInProgress());
    QVERIFY(!directMediaNavigationProvider.loadAt(2).canceled);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));

    directMediaNavigationProvider.finishLoad(2, candidates);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, currentClip);
}

void TestKiriDocumentSessionDeletion::
    pendingDirectMediaDeletionCandidateLoadIsCanceledBySourceChange()
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
    sourceChangeDuringDirectMediaDeletionStartCannotRetargetDestructiveRequest()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl original = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl replacement = localUrl(QStringLiteral("/second/01.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/first/")), { directMediaNavigationCandidate(original) });
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/second/")), { directMediaNavigationCandidate(replacement) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(original);

    bool replacementSubmitted = false;
    QUrl sourceAtProgressCompletion;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (!session->fileDeletionInProgress()) {
                sourceAtProgressCompletion = session->sourceUrl();
                return;
            }
            if (replacementSubmitted) {
                return;
            }
            replacementSubmitted = true;
            session->setSourceUrl(replacement);
        });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QVERIFY(replacementSubmitted);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
    QCOMPARE(sourceAtProgressCompletion, replacement);
    QCOMPARE(session->sourceUrl(), replacement);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), replacement);
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
    QUrl sourceAtProgressCompletion;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (session->fileDeletionInProgress() || latestSelectionSubmitted) {
                return;
            }
            sourceAtProgressCompletion = session->sourceUrl();
            latestSelectionSubmitted = true;
            session->setSourceUrl(latestImage);
        });

    session->setSourceUrl(supersededImage);

    QVERIFY(latestSelectionSubmitted);
    QCOMPARE(sourceAtProgressCompletion, supersededImage);
    QCOMPARE(session->sourceUrl(), latestImage);
    QCOMPARE(session->imageDocument()->sourceUrl(), latestImage);
    QTRY_VERIFY(imageDataLoader.hasActiveLoadForUrl(latestImage));
    QTRY_VERIFY(!imageDataLoader.hasActiveLoadForUrl(supersededImage));
}

void TestKiriDocumentSessionDeletion::deletionCancellationCallbackCanDestroySession()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl firstVideo = localUrl(QStringLiteral("/first/cancel-destroy-first.mp4"));
    const QUrl replacementVideo
        = localUrl(QStringLiteral("/replacement/cancel-destroy-replacement.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/first/")), { directMediaNavigationCandidate(firstVideo) });
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/replacement/")),
        { directMediaNavigationCandidate(replacementVideo) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(firstVideo);
    fileDeletionProvider.setCancelHook([&session]() { session.reset(); });
    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    KiriDocumentSession* const executingSession = session.get();
    executingSession->setSourceUrl(replacementVideo);

    QVERIFY(!session);
    QVERIFY(fileDeletionProvider.operationAt(0).canceled);
}

void TestKiriDocumentSessionDeletion::deletionCancellationReentryCanStartLatestDeletion()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl firstVideo = localUrl(QStringLiteral("/first/cancel-first.mp4"));
    const QUrl supersededVideo = localUrl(QStringLiteral("/superseded/cancel-superseded.mp4"));
    const QUrl latestVideo = localUrl(QStringLiteral("/latest/cancel-latest.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/first/")), { directMediaNavigationCandidate(firstVideo) });
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/superseded/")),
        { directMediaNavigationCandidate(supersededVideo) });
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/latest/")), { directMediaNavigationCandidate(latestVideo) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(firstVideo);
    bool latestDeletionSubmitted = false;
    fileDeletionProvider.setCancelHook([&]() {
        if (latestDeletionSubmitted) {
            return;
        }
        latestDeletionSubmitted = true;
        session->setSourceUrl(latestVideo);
        session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    });
    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QVERIFY(session->fileDeletionInProgress());

    session->setSourceUrl(supersededVideo);

    QVERIFY(latestDeletionSubmitted);
    QCOMPARE(session->sourceUrl(), latestVideo);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(2));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, latestVideo);
    QVERIFY(session->fileDeletionInProgress());
}
