// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionThumbnail::archiveCollectionThumbnailModelUsesPageCandidateNames()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider imageDocumentPageCandidates;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/thumbnails.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("chapter/01.png"));
    const QUrl secondPage = kiriview::TestSupport::archivePageUrl(
        archiveCollection->rootUrl(), QStringLiteral("extras/clip.mp4"));
    imageDocumentPageCandidates.setOpenedCollectionCandidates(archiveCollection->rootUrl(),
        {
            kiriview::ImageDocumentPageCandidate { firstPage, QStringLiteral("chapter/01.png"),
                kiriview::ImageDocumentPageKind::Image },
            kiriview::ImageDocumentPageCandidate { secondPage, QStringLiteral("extras/clip.mp4"),
                kiriview::ImageDocumentPageKind::Video },
        });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, &dataLoader,
            imageDocumentPageCandidates.provider());

    session->setSourceUrl(archiveUrl);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishBackLoad(QByteArrayLiteral("first"));

    QAbstractItemModel* model = session->activeNavigationThumbnailModel();
    QVERIFY(model != nullptr);
    QTRY_COMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(session->activeNavigationCurrentNumber(), 1);
    QCOMPARE(session->activeNavigationCount(), 2);
    QCOMPARE(model->rowCount(), 2);
    compareThumbnailRow(*session, 0, 1, firstPage, QStringLiteral("chapter/01.png"),
        QStringLiteral("image-x-generic-symbolic"), true);
    compareThumbnailRow(*session, 1, 2, secondPage, QStringLiteral("extras/clip.mp4"),
        QStringLiteral("video-x-generic-symbolic"), false);
    const quint64 navigationGeneration = thumbnailData(
        *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                                             .toULongLong();
    QSignalSpy modelResetSpy(model, &QAbstractItemModel::modelReset);
    QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);

    session->openActiveNavigationAtNumber(2);

    QCOMPARE(modelResetSpy.count(), 0);
    QCOMPARE(dataChangedSpy.count(), 2);
    for (const QList<QVariant>& arguments : dataChangedSpy) {
        QCOMPARE(arguments.at(2).value<QList<int>>(),
            QList<int> { kiriview::ActiveNavigationThumbnailModel::CurrentRole });
    }
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(thumbnailData(
                 *session, 0, kiriview::ActiveNavigationThumbnailModel::NavigationGenerationRole)
                 .toULongLong(),
        navigationGeneration);
    compareThumbnailRow(*session, 0, 1, firstPage, QStringLiteral("chapter/01.png"),
        QStringLiteral("image-x-generic-symbolic"), false);
    compareThumbnailRow(*session, 1, 2, secondPage, QStringLiteral("extras/clip.mp4"),
        QStringLiteral("video-x-generic-symbolic"), true);
}
