// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

namespace {
struct MediaInformationEffectsRecorder
{
    kiriview::MediaInformationEffects effects()
    {
        return {
            [this](const QString& text) { copiedTexts.push_back(text); },
            [this](const QUrl& targetUrl, QPointer<QObject> owner) {
                openedTargets.push_back(targetUrl);
                auto* job = new QObject();
                jobs.push_back(job);
                if (owner != nullptr) {
                    job->setParent(owner.data());
                }
            },
        };
    }

    std::vector<QString> copiedTexts;
    std::vector<QUrl> openedTargets;
    std::vector<QPointer<QObject>> jobs;
};
}

void TestKiriDocumentSessionMediaInformation::emptySessionProjectsUnavailableMediaInformation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(mediaInformation != nullptr);
    QVERIFY(!mediaInformation->available());
    QVERIFY(!mediaInformation->canCopyFilePath());
    QVERIFY(!mediaInformation->canOpenContainingFolder());
    QCOMPARE(mediaInformation->title(), QString());
    QCOMPARE(mediaInformation->summary(), QStringLiteral("No media selected"));
    QCOMPARE(mediaInformation->mediaSectionTitle(), QStringLiteral("Media"));
    QVERIFY(!mediaInformation->hasCameraSection());
    QCOMPARE(mediaInformation->generalRows()->rowCount(), 0);
    QCOMPARE(mediaInformation->mediaRows()->rowCount(), 0);
    QCOMPARE(mediaInformation->cameraRows()->rowCount(), 0);
}

void TestKiriDocumentSessionMediaInformation::imageMediaInformationUsesEmbeddedMetadataAndKnownDimensions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("photo.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            {}, staticImageDataDecoderWithMetadata(testCameraMetadata()));

    session->setSourceUrl(imageUrl);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(imageUrl, QByteArrayLiteral("image bytes")));

    QTRY_COMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(mediaInformation->available());
    QCOMPARE(mediaInformation->title(), QStringLiteral("photo.png"));
    QCOMPARE(mediaInformation->summary(), QStringLiteral("Image, 2×2 px"));
    QCOMPARE(mediaInformation->mediaSectionTitle(), QStringLiteral("Image"));
    QVERIFY(mediaInformation->hasCameraSection());
    QVERIFY(mediaInformation->hasAdvancedSection());
    QVERIFY(mediaInformation->canCopyFilePath());
    QVERIFY(mediaInformation->canOpenContainingFolder());
    QCOMPARE(mediaInformation->generalRows()->rowCount(), 2);
    QCOMPARE(mediaInformation->mediaRows()->rowCount(), 1);
    QCOMPARE(mediaInformation->cameraRows()->rowCount(), 8);
    QCOMPARE(mediaInformation->advancedRows()->rowCount(), 1);
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Type")),
        QStringLiteral("Image"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Path")),
        imagePath);
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->mediaRows(), QStringLiteral("Dimensions")),
        QStringLiteral("2×2 px"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->cameraRows(), QStringLiteral("Camera")),
        QStringLiteral("Kiri Camera Co. KiriCam 1"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->cameraRows(), QStringLiteral("Taken")),
        QStringLiteral("2026-05-31 12:34:56"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->cameraRows(), QStringLiteral("Location")),
        QStringLiteral("37.7749, -122.4194"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->advancedRows(), QStringLiteral("Artist")),
        QStringLiteral("Kiri Tester"));
    QVERIFY(!modelValuesContainCaseInsensitive(
        *mediaInformation->generalRows(), QStringLiteral("placeholder")));
    QVERIFY(!modelValuesContainCaseInsensitive(
        *mediaInformation->mediaRows(), QStringLiteral("placeholder")));
    QVERIFY(!modelValuesContainCaseInsensitive(
        *mediaInformation->cameraRows(), QStringLiteral("placeholder")));
}

void TestKiriDocumentSessionMediaInformation::imageMediaInformationClearsStaleEmbeddedMetadataOnReplacement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstPath = directory.filePath(QStringLiteral("first.png"));
    const QString secondPath = directory.filePath(QStringLiteral("second.png"));
    QVERIFY(writeTestImage(firstPath));
    QVERIFY(writeTestImage(secondPath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl firstUrl = localUrl(firstPath);
    const QUrl secondUrl = localUrl(secondPath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(firstUrl), directMediaNavigationCandidate(secondUrl) });
    kiriview::EmbeddedMetadata metadata = testCameraMetadata();
    kiriview::ImageDataDecoder decoder = [metadata = std::move(metadata)](const QByteArray& data,
                                             const kiriview::ImageDecodeRequest& request) {
        kiriview::StaticDecodedImage decoded
            = kiriview::TestSupport::staticDecodedTestImage(kiriview::TestSupport::testImage(
                request.imageUrl().fileName() == QStringLiteral("first.png") ? 2 : 3, 2));
        if (data == QByteArrayLiteral("first")) {
            decoded.embeddedMetadata = metadata;
        }
        return kiriview::successfulDecodedImageResult(std::move(decoded));
    };
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), nullptr, &dataLoader, {}, std::move(decoder));

    session->setSourceUrl(firstUrl);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(firstUrl, QByteArrayLiteral("first")));
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QVERIFY(session->mediaInformation()->hasCameraSection());

    session->setSourceUrl(secondUrl);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(secondUrl, QByteArrayLiteral("second")));
    QTRY_COMPARE(session->imageDocument()->primaryImageSize(), QSize(3, 2));
    QVERIFY(!session->mediaInformation()->hasCameraSection());
    QVERIFY(!session->mediaInformation()->hasAdvancedSection());
}

void TestKiriDocumentSessionMediaInformation::videoMediaInformationUsesVideoSectionAndNoCameraRows()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip = localUrl(QStringLiteral("/media/clip.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(clip);

    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(mediaInformation->available());
    QCOMPARE(mediaInformation->title(), QStringLiteral("clip.mp4"));
    QCOMPARE(mediaInformation->summary(), QStringLiteral("Video"));
    QCOMPARE(mediaInformation->mediaSectionTitle(), QStringLiteral("Video"));
    QVERIFY(!mediaInformation->hasCameraSection());
    QVERIFY(!mediaInformation->hasAdvancedSection());
    QCOMPARE(mediaInformation->cameraRows()->rowCount(), 0);
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Type")),
        QStringLiteral("Video"));
    QCOMPARE(mediaInformation->mediaRows()->rowCount(), 0);
}

void TestKiriDocumentSessionMediaInformation::mediaInformationDerivesFilenameAndPathFromTargetUrl()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl clip(QStringLiteral("zip:///books/book.zip!/chapter/clip.mp4"));
    directMediaNavigationProvider.setMedia(QUrl(QStringLiteral("zip:///books/book.zip!/chapter/")),
        { directMediaNavigationCandidate(clip) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(clip);

    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(mediaInformation->available());
    QCOMPARE(mediaInformation->title(), QStringLiteral("clip.mp4"));
    QCOMPARE(
        mediaInformationValueForLabel(*mediaInformation->generalRows(), QStringLiteral("Path")),
        QStringLiteral("zip:///books/book.zip!/chapter/clip.mp4"));
    QVERIFY(mediaInformation->canCopyFilePath());
    QVERIFY(mediaInformation->canOpenContainingFolder());
}

void TestKiriDocumentSessionMediaInformation::mediaInformationRowModelsExposeLabelAndValueRoles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("roles.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(imageUrl);

    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QAbstractItemModel* model = session->mediaInformation()->generalRows();
    QVERIFY(model != nullptr);
    QCOMPARE(roleForName(*model, QByteArrayLiteral("label")),
        static_cast<int>(KiriMediaInformationRowModel::LabelRole));
    QCOMPARE(roleForName(*model, QByteArrayLiteral("value")),
        static_cast<int>(KiriMediaInformationRowModel::ValueRole));
    QCOMPARE(mediaInformationRowData(*model, 0, QByteArrayLiteral("label")).toString(),
        QStringLiteral("Type"));
    QCOMPARE(mediaInformationRowData(*model, 0, QByteArrayLiteral("value")).toString(),
        QStringLiteral("Image"));
}

void TestKiriDocumentSessionMediaInformation::unavailableMediaInformationSuppressesEffects()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    MediaInformationEffectsRecorder effects;
    std::unique_ptr<KiriDocumentSession> session = createSessionWithMediaInformationEffects(
        directMediaNavigationProvider, effects.effects());

    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(!mediaInformation->available());

    mediaInformation->copyFilePath();
    mediaInformation->openContainingFolder();

    QVERIFY(effects.copiedTexts.empty());
    QVERIFY(effects.openedTargets.empty());
    QVERIFY(effects.jobs.empty());
}

void TestKiriDocumentSessionMediaInformation::mediaInformationInvokablesDispatchCurrentValuesOnce()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl targetUrl = localUrl(QStringLiteral("/media/current.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(targetUrl) });
    MediaInformationEffectsRecorder effects;
    std::unique_ptr<KiriDocumentSession> session = createSessionWithMediaInformationEffects(
        directMediaNavigationProvider, effects.effects());
    session->setSourceUrl(targetUrl);

    KiriMediaInformation* mediaInformation = session->mediaInformation();
    QVERIFY(mediaInformation->available());
    mediaInformation->copyFilePath();
    mediaInformation->openContainingFolder();

    QCOMPARE(effects.copiedTexts.size(), std::size_t(1));
    QCOMPARE(effects.copiedTexts.front(), QStringLiteral("/media/current.mp4"));
    QCOMPARE(effects.openedTargets.size(), std::size_t(1));
    QCOMPARE(effects.openedTargets.front(), targetUrl);
    QCOMPARE(effects.jobs.size(), std::size_t(1));
    QVERIFY(!effects.jobs.front().isNull());
    QCOMPARE(effects.jobs.front()->parent(), mediaInformation);
    session.reset();
    QVERIFY(effects.jobs.front().isNull());
}

void TestKiriDocumentSessionMediaInformation::
    mediaInformationInvokablesFollowReplacementCurrentness()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl firstUrl = localUrl(QStringLiteral("/media/first.mp4"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/replacement/second.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(firstUrl) });
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/replacement/")),
        { directMediaNavigationCandidate(replacementUrl) });
    MediaInformationEffectsRecorder effects;
    std::unique_ptr<KiriDocumentSession> session = createSessionWithMediaInformationEffects(
        directMediaNavigationProvider, effects.effects());

    session->setSourceUrl(firstUrl);
    session->mediaInformation()->copyFilePath();
    session->mediaInformation()->openContainingFolder();
    session->setSourceUrl(replacementUrl);
    session->mediaInformation()->copyFilePath();
    session->mediaInformation()->openContainingFolder();

    QCOMPARE(effects.copiedTexts.size(), std::size_t(2));
    QCOMPARE(effects.copiedTexts.at(0), QStringLiteral("/media/first.mp4"));
    QCOMPARE(effects.copiedTexts.at(1), QStringLiteral("/replacement/second.mp4"));
    QCOMPARE(effects.openedTargets.size(), std::size_t(2));
    QCOMPARE(effects.openedTargets.at(0), firstUrl);
    QCOMPARE(effects.openedTargets.at(1), replacementUrl);
    QCOMPARE(effects.jobs.size(), std::size_t(2));
    for (const QPointer<QObject>& job : effects.jobs) {
        QVERIFY(!job.isNull());
        QCOMPARE(job->parent(), session->mediaInformation());
    }
}

void TestKiriDocumentSessionMediaInformation::copyFilePathSurvivesSynchronousSessionDestruction()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl targetUrl = localUrl(QStringLiteral("/media/current.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(targetUrl) });

    std::unique_ptr<KiriDocumentSession> session;
    QPointer<KiriMediaInformation> mediaInformation;
    QString copiedText;
    bool effectCompleted = false;
    kiriview::MediaInformationEffects effects;
    effects.copyText = [&](QString effectText) {
        copiedText = std::move(effectText);
        session.reset();
        effectCompleted = true;
    };
    session = createSessionWithMediaInformationEffects(
        directMediaNavigationProvider, std::move(effects));
    session->setSourceUrl(targetUrl);
    mediaInformation = session->mediaInformation();

    mediaInformation->copyFilePath();

    QCOMPARE(copiedText, QStringLiteral("/media/current.mp4"));
    QVERIFY(effectCompleted);
    QVERIFY(mediaInformation.isNull());
}

void TestKiriDocumentSessionMediaInformation::
    openContainingFolderSurvivesSynchronousSessionDestruction()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl targetUrl = localUrl(QStringLiteral("/media/current.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(targetUrl) });

    std::unique_ptr<KiriDocumentSession> session;
    QPointer<KiriMediaInformation> mediaInformation;
    QPointer<QObject> returnedJob;
    QUrl openedTarget;
    bool receivedLiveOwner = false;
    bool ownerAliveAfterReset = true;
    kiriview::MediaInformationEffects effects;
    effects.openContainingFolder = [&](const QUrl& effectTarget, QPointer<QObject> owner) {
        openedTarget = effectTarget;
        receivedLiveOwner = owner.data() == mediaInformation.data();
        returnedJob = new QObject();
        session.reset();
        ownerAliveAfterReset = owner != nullptr;
        if (owner != nullptr) {
            returnedJob->setParent(owner.data());
        }
    };
    session = createSessionWithMediaInformationEffects(
        directMediaNavigationProvider, std::move(effects));
    session->setSourceUrl(targetUrl);
    mediaInformation = session->mediaInformation();

    mediaInformation->openContainingFolder();

    QCOMPARE(openedTarget, targetUrl);
    QVERIFY(receivedLiveOwner);
    QVERIFY(!ownerAliveAfterReset);
    QVERIFY(mediaInformation.isNull());
    QVERIFY(!returnedJob.isNull());
    QVERIFY(returnedJob->parent() == nullptr);
    delete returnedJob.data();
}
