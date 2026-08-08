// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionDeletion::videoDeletionUsesOriginalUrlAndOpensMediaFallback()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl clip = QUrl(QStringLiteral("zip:///path/archive.zip!/chapter/01.mp4"));
    const QUrl fallback = QUrl(QStringLiteral("zip:///path/archive.zip!/chapter/02.jpg"));
    directMediaNavigationProvider.setMedia(
        QUrl(QStringLiteral("zip:///path/archive.zip!/chapter/")),
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(fallback) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, clip);
    QVERIFY(session->fileDeletionInProgress());

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->sourceUrl(), fallback);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
}

void TestKiriDocumentSessionDeletion::
    reentrantSourceSelectionDuringSuccessfulVideoDeletionKeepsLatestSelection()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl deletedClip = localUrl(QStringLiteral("/deleted/01.mp4"));
    const QUrl fallbackClip = localUrl(QStringLiteral("/deleted/02.mp4"));
    const QUrl latestClip = localUrl(QStringLiteral("/latest/01.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/deleted/")),
        { directMediaNavigationCandidate(deletedClip),
            directMediaNavigationCandidate(fallbackClip) });
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/latest/")), { directMediaNavigationCandidate(latestClip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(deletedClip);
    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, deletedClip);

    QUrl sourceAtProgressCompletion;
    bool latestSelectionSubmitted = false;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (session->fileDeletionInProgress() || latestSelectionSubmitted) {
                return;
            }
            sourceAtProgressCompletion = session->sourceUrl();
            latestSelectionSubmitted = true;
            session->setSourceUrl(latestClip);
        });

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(latestSelectionSubmitted);
    QCOMPARE(sourceAtProgressCompletion, fallbackClip);
    QCOMPARE(session->sourceUrl(), latestClip);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), latestClip);
    QVERIFY(!session->fileDeletionInProgress());
}

void TestKiriDocumentSessionDeletion::
    stalePendingNavigationCompletionCannotSupersedeDeletionFallback()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl deletedClip = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl navigationClip = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl deletionFallback = localUrl(QStringLiteral("/media/03.mp4"));
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), &fileDeletionProvider);

    session->setSourceUrl(deletedClip);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    directMediaNavigationProvider.finishLoad(0,
        { directMediaNavigationCandidate(deletedClip),
            directMediaNavigationCandidate(navigationClip) });

    session->openNextActiveNavigation();
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(2));

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(3));
    directMediaNavigationProvider.finishLoad(2,
        { directMediaNavigationCandidate(deletedClip),
            directMediaNavigationCandidate(deletionFallback) });
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    bool staleNavigationDelivered = false;
    QUrl sourceAtProgressCompletion;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (!session->fileDeletionInProgress()) {
                sourceAtProgressCompletion = session->sourceUrl();
            }
        });
    connect(session->videoDocument(), &KiriVideoDocument::sourceUrlChanged, session.get(), [&]() {
        if (staleNavigationDelivered || !session->videoDocument()->sourceUrl().isEmpty()) {
            return;
        }
        staleNavigationDelivered = true;
        directMediaNavigationProvider.deliverIgnoringCancellation(1,
            { directMediaNavigationCandidate(deletedClip),
                directMediaNavigationCandidate(navigationClip) });
    });

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(staleNavigationDelivered);
    QCOMPARE(session->sourceUrl(), deletionFallback);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), deletionFallback);
    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(sourceAtProgressCompletion, deletionFallback);
}

void TestKiriDocumentSessionDeletion::canceledVideoDeletionKeepsCurrentVideo()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mov"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::DeletePermanently);
    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Canceled);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->sourceUrl(), clip);
    QCOMPARE(session->videoDocument()->sourceUrl(), clip);
    QCOMPARE(session->errorString(), QString());
}

void TestKiriDocumentSessionDeletion::canceledVideoDeletionRebindsLiveNavigation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl parentUrl = localUrl(QStringLiteral("/media/"));
    const QUrl clip = localUrl(QStringLiteral("/media/01.mov"));
    const QUrl fallback = localUrl(QStringLiteral("/media/02.mov"));
    directMediaNavigationProvider.setMedia(parentUrl,
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(fallback) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Canceled);
    directMediaNavigationProvider.publishMediaChanges(
        parentUrl, { directMediaNavigationCandidate(fallback) });

    QCOMPARE(session->sourceUrl(), fallback);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
}

void TestKiriDocumentSessionDeletion::failedVideoDeletionPublishesErrorWithProgressCompletion()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mov"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    QSignalSpy progressSpy(session.get(), &KiriDocumentSession::fileDeletionInProgressChanged);
    QString errorDuringCompletion;
    QString errorAtFailureNotification;
    bool deletionInProgressAtFailureNotification = true;
    QObject::connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (!session->fileDeletionInProgress()) {
                errorDuringCompletion = session->errorString();
            }
        });
    QObject::connect(session.get(), &KiriDocumentSession::fileDeletionFailed, session.get(),
        [&](const QString&) {
            errorAtFailureNotification = session->errorString();
            deletionInProgressAtFailureNotification = session->fileDeletionInProgress();
        });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    fileDeletionProvider.finishBackOperation(
        kiriview::FileDeletionResult::Failed, QStringLiteral("delete failed"));

    QCOMPARE(progressSpy.count(), 2);
    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->errorString(), QStringLiteral("delete failed"));
    QCOMPARE(errorDuringCompletion, QStringLiteral("delete failed"));
    QCOMPARE(errorAtFailureNotification, QStringLiteral("delete failed"));
    QVERIFY(!deletionInProgressAtFailureNotification);
}

void TestKiriDocumentSessionDeletion::failedVideoDeletionRebindsLiveNavigation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl parentUrl = localUrl(QStringLiteral("/media/"));
    const QUrl clip = localUrl(QStringLiteral("/media/01.mov"));
    const QUrl fallback = localUrl(QStringLiteral("/media/02.mov"));
    directMediaNavigationProvider.setMedia(parentUrl,
        { directMediaNavigationCandidate(clip), directMediaNavigationCandidate(fallback) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    fileDeletionProvider.finishBackOperation(
        kiriview::FileDeletionResult::Failed, QStringLiteral("delete failed"));
    directMediaNavigationProvider.publishMediaChanges(
        parentUrl, { directMediaNavigationCandidate(fallback) });

    QCOMPARE(session->sourceUrl(), fallback);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
}

void TestKiriDocumentSessionDeletion::
    failedVideoDeletionCompletionAllowsImmediateRetryAndUsesGenericMessage()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/01.mov"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(clip);

    bool retrySubmitted = false;
    QString failureNotification;
    connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (session->fileDeletionInProgress() || retrySubmitted) {
                return;
            }
            retrySubmitted = true;
            session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
        });
    connect(session.get(), &KiriDocumentSession::fileDeletionFailed, session.get(),
        [&](const QString& message) { failureNotification = message; });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Failed);

    QVERIFY(retrySubmitted);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(2));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, clip);
    QVERIFY(session->fileDeletionInProgress());
    QVERIFY(!failureNotification.isEmpty());
    QCOMPARE(failureNotification, session->errorString());
}

void TestKiriDocumentSessionDeletion::staleVideoDeletionCompletionAfterSourceChangeIsIgnored()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    const QUrl firstClip = localUrl(QStringLiteral("/first/01.mov"));
    const QUrl staleFallback = localUrl(QStringLiteral("/first/02.png"));
    const QUrl secondClip = localUrl(QStringLiteral("/second/01.mov"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/first/")),
        { directMediaNavigationCandidate(firstClip),
            directMediaNavigationCandidate(staleFallback) });
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/second/")), { directMediaNavigationCandidate(secondClip) });
    std::unique_ptr<KiriDocumentSession> session
        = createSession(directMediaNavigationProvider, &fileDeletionProvider);
    session->setSourceUrl(firstClip);

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fileDeletionProvider.operationAt(0).request.targetUrl, firstClip);
    QVERIFY(session->fileDeletionInProgress());

    session->setSourceUrl(secondClip);

    QCOMPARE(session->sourceUrl(), secondClip);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(!session->fileDeletionInProgress());
    QVERIFY(fileDeletionProvider.operationAt(0).canceled);

    fileDeletionProvider.deliverOperationAtIgnoringCancellation(
        0, kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(session->sourceUrl(), secondClip);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), secondClip);
    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->errorString(), QString());
}
