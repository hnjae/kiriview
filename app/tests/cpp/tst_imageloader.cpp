// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageloader.h"

#include "archive/mediaentrysourcebackend.h"
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
using kiriview::TestSupport::videoCandidate;

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
    void openedCollectionPredecodeLookupPreservesExactScope();
    void openedCollectionStartsProviderTargetBeforeResolvingFirstPage();
    void staleOpenedCollectionSnapshotCannotPrepareAReplacedTarget();
    void reentrantReplacementCannotPublishResolvedStaleVideoTerminal();
    void openedCollectionFailurePreservesTypedSourceDetails();
    void candidateFailurePreservesTypedKioDetails();
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
    std::optional<kiriview::ImageLoadSession> startedSession;
    std::optional<kiriview::ImageLoadSession> preparedSession;
    std::optional<kiriview::PredecodedImage> preparedImage;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.findPredecodedImage
        = [cached](const kiriview::DisplayedImageLocation&) { return cached; };
    callbacks.targetStarted = [&startedSession](kiriview::ImageLoadSession session) {
        startedSession = std::move(session);
    };
    callbacks.resolvedImage = [&preparedSession, &preparedImage](kiriview::ImageLoadSession session,
                                  std::optional<kiriview::PredecodedImage> predecoded) {
        preparedSession = std::move(session);
        preparedImage = std::move(predecoded);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(imageUrl, {})));

    QVERIFY(startedSession.has_value());
    QCOMPARE(startedSession->imageUrl(), imageUrl);
    QVERIFY(preparedSession.has_value());
    QCOMPARE(preparedSession->imageUrl(), imageUrl);
    QVERIFY(preparedImage.has_value());
    QCOMPARE(preparedImage->location, location);
    QCOMPARE(preparedImage->displayImage.image.size(), QSize(24, 12));
}

void TestImageLoader::openedCollectionPredecodeLookupPreservesExactScope()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const QUrl otherArchiveUrl = localUrl(QStringLiteral("/books/other.cbz"));
    const auto scope = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(archiveUrl, {}));
    const auto otherScope = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(otherArchiveUrl, {}));
    QVERIFY(scope.has_value());
    QVERIFY(otherScope.has_value());
    const QUrl sharedEntryUrl = archivePageUrl(scope->rootUrl(), QStringLiteral("same-entry.png"));
    const kiriview::DisplayedImageLocation expectedLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(sharedEntryUrl, *scope);
    const kiriview::DisplayedImageLocation wrongLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(sharedEntryUrl, *otherScope);
    const kiriview::PredecodedImage expected {
        staticDisplayTestImagePayload(testImage(QSize(24, 12))),
        expectedLocation,
    };
    std::vector<kiriview::DisplayedImageLocation> lookups;
    std::optional<kiriview::PredecodedImage> resolvedPredecode;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.findPredecodedImage
        = [&lookups, wrongLocation, expected](const kiriview::DisplayedImageLocation& location)
        -> std::optional<kiriview::PredecodedImage> {
        lookups.push_back(location);
        return location == expected.location
            ? std::optional<kiriview::PredecodedImage>(expected)
            : std::optional<kiriview::PredecodedImage>(kiriview::PredecodedImage {
                  staticDisplayTestImagePayload(testImage(QSize(1, 1))),
                  wrongLocation,
              });
    };
    callbacks.targetStarted = [](kiriview::ImageLoadSession) { };
    callbacks.ensurePageCandidateSnapshot
        = [scope, sharedEntryUrl](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              completion(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                  pageCandidateListSnapshot(
                      kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                          *scope),
                      { imageDocumentPageCandidate(sharedEntryUrl) }),
                  true,
                  {},
              });
          };
    callbacks.sourcePrepared = [](kiriview::ImageLoadSession) { };
    callbacks.resolvedImage = [&resolvedPredecode](kiriview::ImageLoadSession,
                                  std::optional<kiriview::PredecodedImage> predecoded) {
        resolvedPredecode = std::move(predecoded);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QCOMPARE(lookups.size(), std::size_t(1));
    QVERIFY(lookups.front() == expectedLocation);
    QVERIFY(resolvedPredecode.has_value());
    QVERIFY(resolvedPredecode->location == expectedLocation);
    QCOMPARE(resolvedPredecode->displayImage.image.size(), QSize(24, 12));
}

void TestImageLoader::openedCollectionStartsProviderTargetBeforeResolvingFirstPage()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstImageUrl
        = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    std::optional<kiriview::ImageDocumentPageCandidateListSnapshotCallback> pendingSnapshot;
    std::optional<kiriview::ImageLoadSession> targetStarted;
    std::optional<kiriview::ImageLoadSession> sourcePrepared;
    std::optional<kiriview::ImageLoadSession> resolvedImage;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.targetStarted = [&targetStarted](kiriview::ImageLoadSession session) {
        targetStarted = std::move(session);
    };
    callbacks.sourcePrepared = [&sourcePrepared](kiriview::ImageLoadSession session) {
        sourcePrepared = std::move(session);
    };
    callbacks.ensurePageCandidateSnapshot
        = [&pendingSnapshot](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              pendingSnapshot = std::move(completion);
          };
    callbacks.resolvedImage
        = [&resolvedImage](kiriview::ImageLoadSession session, auto predecoded) {
              QVERIFY(!predecoded.has_value());
              resolvedImage = std::move(session);
          };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QVERIFY(targetStarted.has_value());
    QCOMPARE(targetStarted->imageUrl(), archiveUrl);
    QCOMPARE(targetStarted->openedCollectionScope(), *archiveCollection);
    QVERIFY(pendingSnapshot.has_value());
    QVERIFY(!sourcePrepared.has_value());
    QVERIFY(!resolvedImage.has_value());

    (*pendingSnapshot)(kiriview::ImageDocumentPageCandidateListSnapshotResult {
        pageCandidateListSnapshot(
            kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                *archiveCollection),
            { imageDocumentPageCandidate(firstImageUrl) }),
        true,
        {},
    });

    QVERIFY(sourcePrepared.has_value());
    QVERIFY(resolvedImage.has_value());
    QCOMPARE(sourcePrepared->imageUrl(), firstImageUrl);
    QCOMPARE(resolvedImage->imageUrl(), firstImageUrl);
    QCOMPARE(resolvedImage->openedCollectionScope(), *archiveCollection);
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
    std::vector<QUrl> startedUrls;
    std::vector<QUrl> resolvedUrls;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.targetStarted = [&startedUrls](kiriview::ImageLoadSession session) {
        startedUrls.push_back(session.imageUrl());
    };
    callbacks.ensurePageCandidateSnapshot
        = [&pendingSnapshot](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
              pendingSnapshot = std::move(callback);
          };
    callbacks.resolvedImage = [&resolvedUrls](kiriview::ImageLoadSession session, auto) {
        resolvedUrls.push_back(session.imageUrl());
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

    QCOMPARE(startedUrls.size(), std::size_t(2));
    QCOMPARE(startedUrls.front(), archiveUrl);
    QCOMPARE(startedUrls.back(), replacementUrl);
    QCOMPARE(resolvedUrls.size(), std::size_t(1));
    QCOMPARE(resolvedUrls.front(), replacementUrl);
}

void TestImageLoader::reentrantReplacementCannotPublishResolvedStaleVideoTerminal()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/video.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl videoUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.mp4"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/replacement.png"));
    const kiriview::ImageDocumentPageCandidateListSnapshot snapshot = pageCandidateListSnapshot(
        kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            *archiveCollection),
        { videoCandidate(videoUrl) });

    kiriview::ImageLoader* loader = nullptr;
    std::vector<QUrl> startedUrls;
    std::vector<QUrl> resolvedUrls;
    std::vector<QUrl> unsupportedVideoUrls;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.targetStarted = [&startedUrls](kiriview::ImageLoadSession session) {
        startedUrls.push_back(session.imageUrl());
    };
    callbacks.ensurePageCandidateSnapshot
        = [snapshot](auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              completion(
                  kiriview::ImageDocumentPageCandidateListSnapshotResult { snapshot, true, {} });
          };
    callbacks.sourcePrepared = [&loader, replacementUrl](kiriview::ImageLoadSession) {
        QVERIFY(loader != nullptr);
        loader->start(kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(replacementUrl, {})));
    };
    callbacks.unsupportedOpenedCollectionVideo
        = [&unsupportedVideoUrls](kiriview::ImageLoadSession session) {
              unsupportedVideoUrls.push_back(session.imageUrl());
          };
    callbacks.resolvedImage = [&resolvedUrls](kiriview::ImageLoadSession session, auto) {
        resolvedUrls.push_back(session.imageUrl());
    };
    kiriview::ImageLoader imageLoader(std::move(callbacks));
    loader = &imageLoader;

    imageLoader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QCOMPARE(startedUrls.size(), std::size_t(2));
    QCOMPARE(startedUrls.front(), archiveUrl);
    QCOMPARE(startedUrls.back(), replacementUrl);
    QCOMPARE(resolvedUrls.size(), std::size_t(1));
    QCOMPARE(resolvedUrls.front(), replacementUrl);
    QVERIFY(unsupportedVideoUrls.empty());
}

void TestImageLoader::openedCollectionFailurePreservesTypedSourceDetails()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/broken.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const kiriview::MediaEntrySourceError expectedFailure {
        kiriview::MediaEntrySourceErrorCause::CandidateListingFailed,
        kiriview::MediaEntrySourceBackendKind::LibArchive,
        kiriview::MediaEntrySourceOperation::ListCandidates,
        archiveUrl,
        QStringLiteral("nested/chapter.cbz"),
        QStringLiteral("candidate traversal stopped at a malformed nested entry"),
    };
    std::optional<kiriview::ImageLoadFailure> failure;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.targetStarted = [](kiriview::ImageLoadSession) { };
    callbacks.ensurePageCandidateSnapshot
        = [expectedFailure](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              completion(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                  {},
                  false,
                  kiriview::ImageDocumentPageCandidateLoadError { expectedFailure },
              });
          };
    callbacks.error = [&failure](auto, kiriview::ImageLoadFailure loadFailure) {
        failure = std::move(loadFailure);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QVERIFY(failure.has_value());
    QCOMPARE(failure->kind, kiriview::ImageLoadFailureKind::OpenedCollectionLoad);
    QVERIFY(!failure->userMessage.isEmpty());
    QVERIFY(failure->userMessage != expectedFailure.diagnosticDetail);
    QCOMPARE(failure->diagnosticDetail, expectedFailure.diagnosticDetail);
    QVERIFY(failure->mediaEntrySourceError.has_value());
    QCOMPARE(failure->mediaEntrySourceError->cause, expectedFailure.cause);
    QCOMPARE(failure->mediaEntrySourceError->backend, expectedFailure.backend);
    QCOMPARE(failure->mediaEntrySourceError->operation, expectedFailure.operation);
    QCOMPARE(failure->mediaEntrySourceError->collectionUrl, expectedFailure.collectionUrl);
    QCOMPARE(failure->mediaEntrySourceError->entryPath, expectedFailure.entryPath);
    QCOMPARE(failure->mediaEntrySourceError->diagnosticDetail, expectedFailure.diagnosticDetail);
}

void TestImageLoader::candidateFailurePreservesTypedKioDetails()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/broken.cbz"));
    const kiriview::KioOperationFailure expectedFailure {
        kiriview::KioOperationKind::DirectoryListing,
        localUrl(QStringLiteral("/books/")),
        73,
        true,
        QString(),
        QStringLiteral("listing canceled"),
        false,
    };
    std::optional<kiriview::ImageLoadFailure> failure;
    kiriview::ImageLoader::Callbacks callbacks;
    callbacks.targetStarted = [](kiriview::ImageLoadSession) { };
    callbacks.ensurePageCandidateSnapshot
        = [expectedFailure](
              auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback completion) {
              completion(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                  {},
                  false,
                  kiriview::ImageDocumentPageCandidateLoadError { expectedFailure },
              });
          };
    callbacks.error = [&failure](auto, kiriview::ImageLoadFailure loadFailure) {
        failure = std::move(loadFailure);
    };
    kiriview::ImageLoader loader(std::move(callbacks));

    loader.start(kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(archiveUrl, {})));

    QVERIFY(failure.has_value());
    QCOMPARE(failure->kind, kiriview::ImageLoadFailureKind::OpenedCollectionLoad);
    QCOMPARE(failure->userMessage, expectedFailure.userMessage);
    QCOMPARE(failure->diagnosticDetail, expectedFailure.diagnosticDetail);
    QCOMPARE(failure->retryable, expectedFailure.retryable);
    QVERIFY(failure->kioOperationFailure.has_value());
    QCOMPARE(failure->kioOperationFailure->operationKind, expectedFailure.operationKind);
    QCOMPARE(failure->kioOperationFailure->targetUrl, expectedFailure.targetUrl);
    QCOMPARE(failure->kioOperationFailure->rawErrorCode, expectedFailure.rawErrorCode);
    QCOMPARE(failure->kioOperationFailure->canceled, expectedFailure.canceled);
    QCOMPARE(failure->kioOperationFailure->userMessage, expectedFailure.userMessage);
    QCOMPARE(failure->kioOperationFailure->diagnosticDetail, expectedFailure.diagnosticDetail);
    QCOMPARE(failure->kioOperationFailure->retryable, expectedFailure.retryable);
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
