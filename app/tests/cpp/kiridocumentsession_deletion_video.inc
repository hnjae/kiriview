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
    QObject::connect(
        session.get(), &KiriDocumentSession::fileDeletionInProgressChanged, session.get(), [&]() {
            if (!session->fileDeletionInProgress()) {
                errorDuringCompletion = session->errorString();
            }
        });

    session->deleteDisplayedFile(KiriDocumentSession::DeletionMode::MoveToTrash);
    fileDeletionProvider.finishBackOperation(
        kiriview::FileDeletionResult::Failed, QStringLiteral("delete failed"));

    QCOMPARE(progressSpy.count(), 2);
    QVERIFY(!session->fileDeletionInProgress());
    QCOMPARE(session->errorString(), QStringLiteral("delete failed"));
    QCOMPARE(errorDuringCompletion, QStringLiteral("delete failed"));
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
