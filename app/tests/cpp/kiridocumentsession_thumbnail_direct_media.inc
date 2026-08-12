// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionThumbnail::directMediaThumbnailModelTracksSiblingCandidates()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(videoUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(model->rowCount(), 2);
    compareThumbnailRow(*session, 0, 1, imageUrl, QStringLiteral("01.png"),
        QStringLiteral("image-x-generic-symbolic"), false);
    compareThumbnailRow(*session, 1, 2, videoUrl, QStringLiteral("02.mp4"),
        QStringLiteral("video-x-generic-symbolic"), true);
}

void TestKiriDocumentSessionThumbnail::directMediaThumbnailModelStaysEmptyUntilCandidatesAreKnown()
{
    ManualDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider());

    session->setSourceUrl(videoUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QCOMPARE(directMediaNavigationProvider.loadCount(), std::size_t(1));
    QVERIFY(session->activeNavigationAvailable());
    QVERIFY(!session->activeNavigationKnown());
    QCOMPARE(model->rowCount(), 0);

    directMediaNavigationProvider.finishLoad(
        0, { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });

    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(session->activeNavigationCurrentNumber(), 2);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(model->rowCount(), 2);
    compareThumbnailRow(*session, 1, 2, videoUrl, QStringLiteral("02.mp4"),
        QStringLiteral("video-x-generic-symbolic"), true);
}

void TestKiriDocumentSessionThumbnail::activeNavigationThumbnailModelExposesSourceNeutralResultRoles()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/media/")), { directMediaNavigationCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(model->rowCount(), 1);

    QVERIFY(roleForName(*model, QByteArrayLiteral("thumbnailStatus")) >= 0);
    QVERIFY(roleForName(*model, QByteArrayLiteral("thumbnailImageSource")) >= 0);
    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::NoThumbnailResult));
    QCOMPARE(
        thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        QUrl());
}

void TestKiriDocumentSessionThumbnail::activeNavigationThumbnailDemandSurfaceValidatesIdentityAndGeneration()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(videoUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(model->rowCount(), 2);

    const int generationRole = roleForName(*model, QByteArrayLiteral("navigationGeneration"));
    QVERIFY(generationRole >= 0);
    const quint64 generation = model->data(model->index(1, 0), generationRole).toULongLong();
    QVERIFY(generation > 0);

    QVERIFY(!session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(2, videoUrl, 96,
              KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation),
            thumbnailDemand(2, videoUrl, 129,
                KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand,
                generation) }));
    QVERIFY(!session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(2, imageUrl, 256,
            KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand, generation) }));
    QVERIFY(!session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(3, videoUrl, 256,
            KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand, generation) }));
    QVERIFY(!session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(2, videoUrl, 256,
            KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand,
            generation + 1) }));
    QVERIFY(!session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(2, videoUrl, 0,
            KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand, generation) }));
    QVERIFY(
        !session->replaceActiveNavigationThumbnailDemandSnapshot(generation, { QVariantMap {} }));
}

void TestKiriDocumentSessionThumbnail::activeNavigationThumbnailDemandProjectsPendingAndUnsupportedResults()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));
    directMediaNavigationProvider.setMedia(localUrl(QStringLiteral("/media/")),
        { directMediaNavigationCandidate(imageUrl), directMediaNavigationCandidate(videoUrl) });
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QVERIFY(session->activeNavigationKnown());
    QCOMPARE(model->rowCount(), 2);

    const quint64 generation = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                   .toULongLong();
    QVERIFY(generation > 0);

    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(1, imageUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));
    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::PendingThumbnailResult));
    QCOMPARE(
        thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        QUrl());

    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(2, videoUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));
    QCOMPARE(thumbnailDataForRoleName(*session, 1, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::PendingThumbnailResult));
    QCOMPARE(
        thumbnailDataForRoleName(*session, 1, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        QUrl());

    const QUrl remoteDirectoryUrl(QStringLiteral("https://example.invalid/media/"));
    const QUrl remoteImageUrl(QStringLiteral("https://example.invalid/media/01.png"));
    const QUrl remoteVideoUrl(QStringLiteral("https://example.invalid/media/02.mp4"));
    FakeDirectMediaNavigationCandidateProvider remoteDirectMediaNavigationProvider;
    remoteDirectMediaNavigationProvider.setMedia(remoteDirectoryUrl,
        { directMediaNavigationCandidate(remoteImageUrl),
            directMediaNavigationCandidate(remoteVideoUrl) });
    kiriview::TestSupport::ManualImageDataLoader remoteImageDataLoader;
    std::unique_ptr<KiriDocumentSession> remoteSession
        = createSession(remoteDirectMediaNavigationProvider, nullptr, &remoteImageDataLoader);

    remoteSession->setSourceUrl(remoteImageUrl);

    QAbstractItemModel* remoteModel = remoteSession->activeNavigationThumbnailModel();
    QVERIFY(remoteModel != nullptr);
    QVERIFY(remoteSession->activeNavigationKnown());
    QCOMPARE(remoteModel->rowCount(), 2);

    const quint64 remoteGeneration = thumbnailData(
        *remoteSession, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                         .toULongLong();
    QVERIFY(remoteGeneration > 0);

    QVERIFY(remoteSession->replaceActiveNavigationThumbnailDemandSnapshot(remoteGeneration,
        { thumbnailDemand(2, remoteVideoUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand,
            remoteGeneration) }));
    QCOMPARE(
        thumbnailDataForRoleName(*remoteSession, 1, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::UnsupportedThumbnailResult));
    QCOMPARE(thumbnailDataForRoleName(*remoteSession, 1, QByteArrayLiteral("thumbnailImageSource"))
                 .toUrl(),
        QUrl());
}

void TestKiriDocumentSessionThumbnail::directImageThumbnailDemandProjectsReadyCacheHitSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeThumbnailLookupProvider thumbnailLookup;
    QImage thumbnail(QSize(2, 1), QImage::Format_RGBA8888);
    thumbnail.fill(Qt::blue);
    thumbnailLookup.result = kiriview::ThumbnailCacheLookupResult {
        kiriview::ThumbnailCacheLookupStatus::Ready,
        thumbnail,
        kiriview::ActiveNavigationThumbnailDemandBucket::Normal,
        kiriview::ActiveNavigationThumbnailDemandBucket::Normal,
        QStringLiteral("/cache/thumbnail.png"),
        {},
    };
    auto store = std::make_shared<kiriview::ThumbnailImageStore>();
    std::unique_ptr<KiriDocumentSession> session = createSessionWithProvider(
        directMediaNavigationProvider.provider(), nullptr, nullptr, {},
        kiriview::TestSupport::staticImageDataDecoder(), {}, thumbnailLookup.provider(), {}, store);

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(model->rowCount(), 1);

    const quint64 generation = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                   .toULongLong();
    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(1, imageUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));

    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(4));
    QCOMPARE(thumbnailLookup.requests.front().localPathBytes, QFile::encodeName(imagePath));
    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::ReadyThumbnailResult));
    const QUrl source
        = thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl();
    QVERIFY(source.toString().startsWith(QStringLiteral("image://kiriview-thumbnails/")));
    QCOMPARE(store->size(), qsizetype(1));
}

void TestKiriDocumentSessionThumbnail::directImageThumbnailDemandProjectsReadyGeneratedSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeThumbnailLookupProvider thumbnailLookup;
    thumbnailLookup.result.status = kiriview::ThumbnailCacheLookupStatus::Missing;
    FakeThumbnailGenerationProvider thumbnailGeneration;
    QImage thumbnail(QSize(2, 1), QImage::Format_RGBA8888);
    thumbnail.fill(Qt::green);
    thumbnailGeneration.result = kiriview::ThumbnailGenerationResult {
        kiriview::ThumbnailGenerationStatus::Ready,
        {},
        thumbnail,
        kiriview::ActiveNavigationThumbnailDemandBucket::Normal,
        QStringLiteral("/cache/generated.png"),
        {},
    };
    auto store = std::make_shared<kiriview::ThumbnailImageStore>();
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, thumbnailLookup.provider(),
            thumbnailGeneration.provider(), store);

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    const quint64 generation = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                   .toULongLong();
    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(1, imageUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));

    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(4));
    QCOMPARE(thumbnailGeneration.requests.size(), std::size_t(4));
    QCOMPARE(thumbnailGeneration.requests.front().localPathBytes, QFile::encodeName(imagePath));
    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::ReadyThumbnailResult));
    const QUrl source
        = thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl();
    QVERIFY(source.toString().startsWith(QStringLiteral("image://kiriview-thumbnails/")));
    QCOMPARE(store->size(), qsizetype(1));
}

void TestKiriDocumentSessionThumbnail::directImageThumbnailDemandKeepsFallbackForFailedLookup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeThumbnailLookupProvider thumbnailLookup;
    thumbnailLookup.result.status = kiriview::ThumbnailCacheLookupStatus::Failed;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, thumbnailLookup.provider());

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    const quint64 generation = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                   .toULongLong();
    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(1, imageUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));

    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::FailedThumbnailResult));
    QCOMPARE(
        thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        QUrl());
}

void TestKiriDocumentSessionThumbnail::directImageThumbnailDemandKeepsFallbackForFailedGeneration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("01.png"));
    QVERIFY(writeTestImage(imagePath));

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    const QUrl imageUrl = localUrl(imagePath);
    directMediaNavigationProvider.setMedia(localUrl(directory.path() + QStringLiteral("/")),
        { directMediaNavigationCandidate(imageUrl) });
    FakeThumbnailLookupProvider thumbnailLookup;
    thumbnailLookup.result.status = kiriview::ThumbnailCacheLookupStatus::Missing;
    FakeThumbnailGenerationProvider thumbnailGeneration;
    thumbnailGeneration.result.status = kiriview::ThumbnailGenerationStatus::Failed;
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), {}, thumbnailLookup.provider(),
            thumbnailGeneration.provider());

    session->setSourceUrl(imageUrl);

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    const quint64 generation = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                   .toULongLong();
    QVERIFY(session->replaceActiveNavigationThumbnailDemandSnapshot(generation,
        { thumbnailDemand(1, imageUrl, 96,
            KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand, generation) }));

    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(4));
    QCOMPARE(thumbnailGeneration.requests.size(), std::size_t(4));
    QCOMPARE(thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(KiriDocumentSession::ThumbnailResultStatus::FailedThumbnailResult));
    QCOMPARE(
        thumbnailDataForRoleName(*session, 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        QUrl());
}
