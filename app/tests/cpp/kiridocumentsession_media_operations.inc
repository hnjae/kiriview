// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionMediaOperations::openWithIsUnavailableInEmptySession()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    FakeMediaOpenWithProvider openWithProvider;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider());

    QVERIFY(!session->displayedMediaOpenWithAvailable());

    session->openCurrentMediaWith();

    QVERIFY(openWithProvider.requests.empty());
}

void TestKiriDocumentSessionMediaOperations::openWithUsesCurrentDirectImageUrl()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));
    const QUrl imageUrl = localUrl(imagePath);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeMediaOpenWithProvider openWithProvider;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider());

    QSignalSpy availabilitySpy(
        session.get(), &KiriDocumentSession::displayedMediaOpenWithAvailabilityChanged);
    session->setSourceUrl(imageUrl);

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QVERIFY(session->displayedMediaOpenWithAvailable());
    QVERIFY(availabilitySpy.count() > 0);

    session->openCurrentMediaWith();

    QCOMPARE(openWithProvider.requests.size(), std::size_t(1));
    QCOMPARE(openWithProvider.requests.at(0).targetUrl, imageUrl);
}

void TestKiriDocumentSessionMediaOperations::openWithFailureEmitsToastSignal()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));
    const QUrl imageUrl = localUrl(imagePath);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeMediaOpenWithProvider openWithProvider;
    openWithProvider.result = kiriview::MediaOpenWithResult::Failed;
    openWithProvider.errorString = QStringLiteral("launcher failed");
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider());
    QSignalSpy failureSpy(session.get(), &KiriDocumentSession::openWithFailed);

    session->setSourceUrl(imageUrl);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    session->openCurrentMediaWith();

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.at(0).at(0).toString(), QStringLiteral("launcher failed"));
}

void TestKiriDocumentSessionMediaOperations::staleOpenWithFailureAfterReplacementIsIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstPath = directory.filePath(QStringLiteral("01.png"));
    const QString secondPath = directory.filePath(QStringLiteral("02.png"));
    QVERIFY(writeTestImage(firstPath));
    QVERIFY(writeTestImage(secondPath));
    const QUrl firstUrl = localUrl(firstPath);
    const QUrl secondUrl = localUrl(secondPath);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(firstUrl), directMediaNavigationCandidate(secondUrl) });
    ManualMediaOpenWithProvider openWithProvider;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider());
    QSignalSpy failureSpy(session.get(), &KiriDocumentSession::openWithFailed);

    session->setSourceUrl(firstUrl);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    session->openCurrentMediaWith();
    QCOMPARE(openWithProvider.operationCount(), std::size_t(1));

    session->setSourceUrl(secondUrl);
    QTRY_COMPARE(session->imageDocument()->displayedUrl(), secondUrl);

    openWithProvider.deliverOperationAtIgnoringCancellation(
        0, kiriview::MediaOpenWithResult::Failed, QStringLiteral("stale launcher failed"));

    QCOMPARE(failureSpy.count(), 0);
}

void TestKiriDocumentSessionMediaOperations::staleOpenWithFailureAfterSessionDestructionIsIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));
    const QUrl imageUrl = localUrl(imagePath);

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    ManualMediaOpenWithProvider openWithProvider;
    int failureCount = 0;

    {
        std::unique_ptr<KiriDocumentSession> session
            = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr,
                {}, kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider());
        QObject::connect(session.get(), &KiriDocumentSession::openWithFailed, session.get(),
            [&failureCount]() { ++failureCount; });

        session->setSourceUrl(imageUrl);
        QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
        session->openCurrentMediaWith();
        QCOMPARE(openWithProvider.operationCount(), std::size_t(1));
    }

    QVERIFY(openWithProvider.operationAt(0).canceled);
    openWithProvider.deliverOperationAtIgnoringCancellation(
        0, kiriview::MediaOpenWithResult::Failed, QStringLiteral("destroyed launcher failed"));

    QCOMPARE(failureCount, 0);
}

void TestKiriDocumentSessionMediaOperations::videoNavigationReusesStillImageWarmCacheWhenReturning()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl firstImage = localUrl(QStringLiteral("/media/00.png"));
    const QUrl video = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextImage = localUrl(QStringLiteral("/media/02.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstImage), directMediaNavigationCandidate(video),
            directMediaNavigationCandidate(nextImage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, nullptr, &imageDataLoader);

    session->setSourceUrl(firstImage);
    QTRY_COMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, firstImage);
    imageDataLoader.finishFrontLoad(QByteArrayLiteral("first"));
    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);

    QTRY_COMPARE(imageDataLoader.loadCount(), std::size_t(2));
    QCOMPARE(imageDataLoader.backLoad().url, nextImage);
    imageDataLoader.finishBackLoad(QByteArrayLiteral("next"));

    session->openNextActiveNavigation();
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), video);

    const std::size_t loadCountBeforeReturn = imageDataLoader.loadCount();
    session->openPreviousActiveNavigation();

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), firstImage);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(imageDataLoader.loadCount(), loadCountBeforeReturn);
}

void TestKiriDocumentSessionMediaOperations::videoActiveNavigationExposesCurrentNumberAndCount()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(videoUrl);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QVERIFY(session->canOpenPreviousActiveNavigation());
    QVERIFY(!session->canOpenNextActiveNavigation());

    session->openActiveNavigationAtNumber(1);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), imageUrl);
    QCOMPARE(session->imageDocument()->sourceUrl(), imageUrl);
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
}

void TestKiriDocumentSessionMediaOperations::initialDirectImagePredecodeUsesRequestedMediaCursor()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    const QUrl firstImage = localUrl(QStringLiteral("/media/01.png"));
    const QUrl video = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl nextImage = localUrl(QStringLiteral("/media/03.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(firstImage), directMediaNavigationCandidate(video),
            directMediaNavigationCandidate(nextImage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, nullptr, &imageDataLoader);

    session->setSourceUrl(firstImage);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, firstImage);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QTRY_COMPARE(imageDataLoader.loadCount(), std::size_t(2));
    QCOMPARE(imageDataLoader.backLoad().url, nextImage);
}

void TestKiriDocumentSessionMediaOperations::directImagePredecodeUsesSessionDependencyOverrides()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    kiriview::TestSupport::ManualImageDataLoader directMediaPredecodeDataLoader;
    const QUrl currentImage = localUrl(QStringLiteral("/media/current.png"));
    const QUrl nextImage = localUrl(QStringLiteral("/media/next.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(currentImage),
            directMediaNavigationCandidate(nextImage) });

    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.sessionRuntime.directMediaNavigationCandidateProvider
        = directMediaNavigationProvider.provider();
    dependencies.imageDocument.imageDecode = kiriview::TestSupport::imageDecodeDependenciesFor(
        imageDataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.sessionRuntime.directMediaPredecodeDependencies.imageDecode
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            directMediaPredecodeDataLoader, kiriview::TestSupport::staticImageDataDecoder());
    std::unique_ptr<KiriDocumentSession> session
        = std::make_unique<KiriDocumentSession>(std::move(dependencies));
    attachTestViewport(*session);

    session->setSourceUrl(currentImage);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, currentImage);
    QTRY_COMPARE(directMediaPredecodeDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(directMediaPredecodeDataLoader.frontLoad().url, nextImage);
    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
}

void TestKiriDocumentSessionMediaOperations::composedDependenciesConfigureSharedThumbnailStoreBudgetFromSnapshot()
{
    constexpr qsizetype physicalByteSize = 1024 * 1024 * 1024;
    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.imageDocument.systemMemorySnapshot
        = kiriview::SystemMemorySnapshot { physicalByteSize };

    [[maybe_unused]] KiriDocumentSession session(std::move(dependencies));

    QCOMPARE(kiriview::sharedThumbnailImageStore()->byteBudget(), physicalByteSize / 64);
}

void TestKiriDocumentSessionMediaOperations::directImagePredecodeDoesNotUseImageDocumentPageCandidates()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader imageDataLoader;
    kiriview::TestSupport::ManualImageDataLoader directMediaPredecodeDataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler directMediaPredecodeWorkerScheduler;
    kiriview::TestSupport::ManualTimerScheduler directMediaPredecodeTimerScheduler;
    kiriview::TestSupport::ManualTimerScheduler imageDocumentPredecodeTimerScheduler;
    const QUrl currentImage = localUrl(QStringLiteral("/media/current.png"));
    const QUrl directMediaNextImage = localUrl(QStringLiteral("/media/next.png"));
    const QUrl imageDocumentOnlyImage = localUrl(QStringLiteral("/media/page-only.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(currentImage),
            directMediaNavigationCandidate(directMediaNextImage) });
    imageDocumentPageCandidates.setDirectoryImages(localUrl(QStringLiteral("/media/")),
        { kiriview::TestSupport::imageDocumentPageCandidate(currentImage),
            kiriview::TestSupport::imageDocumentPageCandidate(imageDocumentOnlyImage) });
    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.sessionRuntime.directMediaNavigationCandidateProvider
        = directMediaNavigationProvider.provider();
    dependencies.imageDocument.candidateProvider = imageDocumentPageCandidates.provider();
    dependencies.imageDocument.imageDecode = kiriview::TestSupport::imageDecodeDependenciesFor(
        imageDataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.imageDocument.predecodeTimerScheduler
        = imageDocumentPredecodeTimerScheduler.scheduler();
    dependencies.sessionRuntime.directMediaPredecodeDependencies.imageDecode
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            directMediaPredecodeDataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.sessionRuntime.directMediaPredecodeDependencies.imageDecode.workerScheduler
        = directMediaPredecodeWorkerScheduler.scheduler();
    dependencies.sessionRuntime.directMediaPredecodeDependencies.timerScheduler
        = directMediaPredecodeTimerScheduler.scheduler();
    std::unique_ptr<KiriDocumentSession> session
        = std::make_unique<KiriDocumentSession>(std::move(dependencies));
    attachTestViewport(*session);

    session->setSourceUrl(currentImage);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(imageDataLoader.frontLoad().url, currentImage);
    QTRY_VERIFY(directMediaPredecodeTimerScheduler.timerCount() >= 2);
    QVERIFY(directMediaPredecodeTimerScheduler.timerAt(0).active());
    directMediaPredecodeTimerScheduler.timerAt(0).fire();
    QTRY_COMPARE(directMediaPredecodeDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(directMediaPredecodeDataLoader.frontLoad().url, directMediaNextImage);
    directMediaPredecodeDataLoader.finishFrontLoad(QByteArrayLiteral("direct-next"));
    QCOMPARE(directMediaPredecodeWorkerScheduler.scheduleCount(), std::size_t(1));
    directMediaPredecodeWorkerScheduler.runWork(0);
    directMediaPredecodeWorkerScheduler.finish(0);
    QVERIFY(
        imageDataLoader.finishOldestActiveLoadForUrl(currentImage, QByteArrayLiteral("current")));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);

    QCOMPARE(imageDataLoader.loadCount(), std::size_t(1));
    QCOMPARE(directMediaPredecodeDataLoader.loadCount(), std::size_t(1));
    for (std::size_t index = 0; index < imageDocumentPredecodeTimerScheduler.timerCount();
        ++index) {
        QVERIFY(!imageDocumentPredecodeTimerScheduler.timerAt(index).active());
    }
}

void TestKiriDocumentSessionMediaOperations::
    staleDirectMediaNavigationCandidateCompletionCannotPublishForNewSource()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider());

    const QUrl firstClip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl secondClip = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl secondSibling = localUrl(QStringLiteral("/media/03.mp4"));

    session->setSourceUrl(firstClip);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QCOMPARE(
        directMediaNavigationProvider.loadAt(0).parentUrl, localUrl(QStringLiteral("/media/")));

    session->setSourceUrl(secondClip);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));
    QCOMPARE(
        directMediaNavigationProvider.loadAt(1).parentUrl, localUrl(QStringLiteral("/media/")));
    QVERIFY(directMediaNavigationProvider.loadAt(0).canceled);
    QVERIFY(!session->activeNavigationKnown());

    directMediaNavigationProvider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(secondClip),
            directMediaNavigationCandidate(secondSibling) });

    QVERIFY(!session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 0);
    QCOMPARE(session->activeNavigationCount(), 0);

    directMediaNavigationProvider.finishLoad(1,
        { directMediaNavigationCandidate(firstClip), directMediaNavigationCandidate(secondClip) });

    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
}

void TestKiriDocumentSessionMediaOperations::
    nextMediaFromVideoCanRouteToImageWithoutUsingImageDocumentPageNavigation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl image = localUrl(QStringLiteral("/media/02.png"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(image) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);
    session->setSourceUrl(clip);

    QVERIFY(session->canOpenNextActiveNavigation());
    session->openNextActiveNavigation();

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->sourceUrl(), image);
    QCOMPARE(session->imageDocument()->sourceUrl(), image);
    QCOMPARE(session->videoDocument()->sourceUrl(), QUrl());
}
