// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageloader.h"

#include "image_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::testImage;

kiriview::ImageDocumentPageCandidateListSnapshot pageCandidateListSnapshot(
    kiriview::ImageDocumentPageCandidateListSource source,
    kiriview::ImageDocumentPageCandidateRows candidates)
{
    kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.revision = 1;
    snapshot.candidates
        = std::make_shared<const kiriview::ImageDocumentPageCandidateRows>(std::move(candidates));
    snapshot.known = true;
    return snapshot;
}
}

class TestImageLoader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directImagePreparesProviderTargetWithValidatedPredecode();
    void openedCollectionResolvesFirstPageBeforePreparingProviderTarget();
    void staleOpenedCollectionSnapshotCannotPrepareAReplacedTarget();
    void missingProviderTargetOwnerReportsTypedPresentationFailure();
};

void TestImageLoader::directImagePreparesProviderTargetWithValidatedPredecode()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/images/page.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(imageUrl);
    const kiriview::PredecodedImage cached {
        staticDisplayTestImagePayload(testImage(QSize(24, 12))),
        location,
    };
    std::optional<kiriview::ImageLoadSession> preparedSession;
    std::optional<kiriview::PredecodedImage> preparedImage;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.findPredecodedImage = [cached](const QUrl&) { return cached; };
    callbacks.preparedImage = [&preparedSession, &preparedImage](kiriview::ImageLoadSession session,
                                  std::optional<kiriview::PredecodedImage> predecoded) {
        preparedSession = std::move(session);
        preparedImage = std::move(predecoded);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(imageUrl, {})));

    QVERIFY(preparedSession.has_value());
    QCOMPARE(preparedSession->imageUrl(), imageUrl);
    QVERIFY(preparedImage.has_value());
    QCOMPARE(preparedImage->location, location);
    QCOMPARE(preparedImage->displayImage.image.size(), QSize(24, 12));
}

void TestImageLoader::openedCollectionResolvesFirstPageBeforePreparingProviderTarget()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstImageUrl
        = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const kiriview::ImageDocumentPageCandidateListSnapshot snapshot = pageCandidateListSnapshot(
        kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            *archiveCollection),
        { imageDocumentPageCandidate(firstImageUrl) });

    std::optional<kiriview::ImageLoadSession> sourcePrepared;
    std::optional<kiriview::ImageLoadSession> providerPrepared;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.sourcePrepared = [&sourcePrepared](kiriview::ImageLoadSession session) {
        sourcePrepared = std::move(session);
    };
    callbacks.ensurePageCandidateSnapshot
        = [snapshot](auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              completion(
                  kiriview::ImageDocumentPageCandidateListSnapshotResult { snapshot, true, {} });
          };
    callbacks.preparedImage
        = [&providerPrepared](kiriview::ImageLoadSession session, auto predecoded) {
              QVERIFY(!predecoded.has_value());
              providerPrepared = std::move(session);
          };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QVERIFY(sourcePrepared.has_value());
    QVERIFY(providerPrepared.has_value());
    QCOMPARE(sourcePrepared->imageUrl(), firstImageUrl);
    QCOMPARE(providerPrepared->imageUrl(), firstImageUrl);
    QCOMPARE(providerPrepared->openedCollectionScope(), *archiveCollection);
}

void TestImageLoader::staleOpenedCollectionSnapshotCannotPrepareAReplacedTarget()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl staleImageUrl
        = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/replacement.png"));
    std::optional<kiriview::ImageDocumentPageCandidateListSnapshotCallback> pendingSnapshot;
    std::vector<QUrl> preparedUrls;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.ensurePageCandidateSnapshot
        = [&pendingSnapshot](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
              pendingSnapshot = std::move(callback);
          };
    callbacks.preparedImage = [&preparedUrls](kiriview::ImageLoadSession session, auto) {
        preparedUrls.push_back(session.imageUrl());
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));
    QVERIFY(pendingSnapshot.has_value());
    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(replacementUrl, {})));

    (*pendingSnapshot)(kiriview::ImageDocumentPageCandidateListSnapshotResult {
        pageCandidateListSnapshot(
            kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                *archiveCollection),
            { imageDocumentPageCandidate(staleImageUrl) }),
        true,
        {},
    });

    QCOMPARE(preparedUrls.size(), std::size_t(1));
    QCOMPARE(preparedUrls.front(), replacementUrl);
}

void TestImageLoader::missingProviderTargetOwnerReportsTypedPresentationFailure()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/images/page.png"));
    std::optional<kiriview::ImageLoadFailure> failure;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.error = [&failure](auto, kiriview::ImageLoadFailure loadFailure) {
        failure = std::move(loadFailure);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(imageUrl, {})));

    QVERIFY(failure.has_value());
    QCOMPARE(failure->sourceUrl, imageUrl);
    QCOMPARE(failure->kind, kiriview::ImageLoadFailureKind::Presentation);
}

QTEST_GUILESS_MAIN(TestImageLoader)

#include "tst_imageloader.moc"
