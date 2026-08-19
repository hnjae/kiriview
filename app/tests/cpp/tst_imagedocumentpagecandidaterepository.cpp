// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidaterepository.h"

#include "candidate_test_support.h"
#include "location/imagedocumentlocation.h"
#include "navigation/imagedocumentpagecandidateprovider.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
using kiriview::ContainerNavigationCandidate;
using kiriview::ContainerNavigationCandidateType;
using kiriview::ImageDocumentPageCandidate;
using kiriview::ImageDocumentPageCandidateListSource;
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::containerCandidate;
using kiriview::TestSupport::FakeImageDocumentPageCandidateProvider;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::localUrl;

void compareSingleImageDocumentPageCandidate(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& url)
{
    QCOMPARE(candidates.size(), std::size_t(1));
    QCOMPARE(candidates.front().url, url);
}

void compareSingleContainerCandidate(
    const std::vector<ContainerNavigationCandidate>& candidates, const QUrl& url)
{
    QCOMPARE(candidates.size(), std::size_t(1));
    QCOMPARE(candidates.front().url, url);
}
}

class TestImageDocumentPageCandidateRepository : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadImagesRoutesDirectorySources();
    void loadImagesPreservesTypedDirectoryFailure();
    void loadImagesRejectsForeignProviderCandidatesAtomically();
    void loadImagesRoutesArchiveSources();
    void openedCollectionEntryFactsMapKindsAndSortAtNavigationBoundary();
    void loadContainersRoutesDirectoryContainerSources();
    void watchCandidateChangesRoutesDirectorySources();
};

void TestImageDocumentPageCandidateRepository::
    loadImagesRejectsForeignProviderCandidatesAtomically()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/books/a/"));
    fakeProvider.setDirectoryImages(directoryUrl,
        { imageDocumentPageCandidate(localUrl(QStringLiteral("/books/a/01.png"))),
            imageDocumentPageCandidate(localUrl(QStringLiteral("/foreign/02.png"))) });

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    int successCount = 0;
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> actualError;
    repository.loadImages(
        nullptr, ImageDocumentPageCandidateListSource::forDirectory(directoryUrl),
        [&successCount](std::vector<ImageDocumentPageCandidate>) { ++successCount; },
        [&actualError](kiriview::ImageDocumentPageCandidateLoadError error) {
            actualError = std::move(error);
        });

    QCOMPARE(successCount, 0);
    QVERIFY(actualError.has_value());
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&*actualError);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::KioOperationFailureCause::Validation);
    QCOMPARE(failure->targetUrl, directoryUrl);
}

void TestImageDocumentPageCandidateRepository::loadImagesRoutesDirectorySources()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/books/a/"));
    const QUrl imageUrl = localUrl(QStringLiteral("/books/a/01.png"));
    fakeProvider.setDirectoryImages(directoryUrl, { imageDocumentPageCandidate(imageUrl) });

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    std::vector<ImageDocumentPageCandidate> loadedCandidates;
    repository.loadImages(nullptr, ImageDocumentPageCandidateListSource::forDirectory(directoryUrl),
        [&loadedCandidates](std::vector<ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
        },
        {});

    compareSingleImageDocumentPageCandidate(loadedCandidates, imageUrl);
}

void TestImageDocumentPageCandidateRepository::loadImagesPreservesTypedDirectoryFailure()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/books/a/"));
    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        directoryUrl,
        73,
        true,
        QString(),
        QStringLiteral("listing canceled"),
        false,
    };
    fakeProvider.setDirectoryImageFailure(directoryUrl, expected);

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> actualError;
    repository.loadImages(nullptr, ImageDocumentPageCandidateListSource::forDirectory(directoryUrl),
        {}, [&actualError](kiriview::ImageDocumentPageCandidateLoadError error) {
            actualError = std::move(error);
        });

    QVERIFY(actualError.has_value());
    const auto* actual = std::get_if<kiriview::KioOperationFailure>(&*actualError);
    QVERIFY(actual != nullptr);
    QCOMPARE(actual->operationKind, expected.operationKind);
    QCOMPARE(actual->targetUrl, expected.targetUrl);
    QCOMPARE(actual->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(actual->canceled, expected.canceled);
    QCOMPARE(actual->userMessage, expected.userMessage);
    QCOMPARE(actual->diagnosticDetail, expected.diagnosticDetail);
    QCOMPARE(actual->retryable, expected.retryable);
}

void TestImageDocumentPageCandidateRepository::loadImagesRoutesArchiveSources()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());
    const QUrl imageUrl
        = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("01.png"));
    fakeProvider.setOpenedCollectionCandidates(
        openedCollectionScope->rootUrl(), { imageDocumentPageCandidate(imageUrl) });

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    std::vector<ImageDocumentPageCandidate> loadedCandidates;
    repository.loadImages(nullptr,
        ImageDocumentPageCandidateListSource::forOpenedCollectionScope(*openedCollectionScope),
        [&loadedCandidates](std::vector<ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
        },
        {});

    compareSingleImageDocumentPageCandidate(loadedCandidates, imageUrl);
}

void TestImageDocumentPageCandidateRepository::
    openedCollectionEntryFactsMapKindsAndSortAtNavigationBoundary()
{
    const QUrl rootUrl(QStringLiteral("zip:///books/book.cbz/"));
    kiriview::ImageDocumentPageCandidateProvider provider
        = kiriview::imageDocumentPageCandidateProviderWithOpenedCollectionEntryLoader({},
            [rootUrl](QObject*, kiriview::OpenedCollectionScopeLocation,
                kiriview::MediaEntrySourceEntriesCallback callback,
                kiriview::MediaEntrySourceErrorCallback) {
                callback({
                    { rootUrl.resolved(QUrl(QStringLiteral("b.png"))), QStringLiteral("b.png"),
                        kiriview::MediaEntrySourceEntryKind::Image },
                    { rootUrl.resolved(QUrl(QStringLiteral("a.mp4"))), QStringLiteral("a.mp4"),
                        kiriview::MediaEntrySourceEntryKind::Video },
                });
                return kiriview::ImageIoJob {};
            });
    std::vector<ImageDocumentPageCandidate> candidates;

    provider.openedCollectionCandidates(nullptr, {},
        [&candidates](
            std::vector<ImageDocumentPageCandidate> loaded) { candidates = std::move(loaded); },
        {});

    QCOMPARE(candidates.size(), std::size_t(2));
    QCOMPARE(candidates.at(0).name, QStringLiteral("a.mp4"));
    QCOMPARE(candidates.at(0).kind, kiriview::ImageDocumentPageKind::Video);
    QCOMPARE(candidates.at(1).name, QStringLiteral("b.png"));
    QCOMPARE(candidates.at(1).kind, kiriview::ImageDocumentPageKind::Image);
}

void TestImageDocumentPageCandidateRepository::loadContainersRoutesDirectoryContainerSources()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/books/"));
    const QUrl containerUrl = localUrl(QStringLiteral("/books/a/"));
    fakeProvider.setContainerCandidates(directoryUrl,
        { containerCandidate(containerUrl, ContainerNavigationCandidateType::Directory) });

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    std::vector<ContainerNavigationCandidate> loadedCandidates;
    repository.loadContainers(nullptr, directoryUrl,
        [&loadedCandidates](std::vector<ContainerNavigationCandidate> candidates) {
            loadedCandidates = std::move(candidates);
        },
        {});

    compareSingleContainerCandidate(loadedCandidates, containerUrl);
}

void TestImageDocumentPageCandidateRepository::watchCandidateChangesRoutesDirectorySources()
{
    FakeImageDocumentPageCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/books/a/"));
    const QUrl imageUrl = localUrl(QStringLiteral("/books/a/01.png"));
    QObject receiver;

    kiriview::ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    std::vector<ImageDocumentPageCandidate> changedCandidates;
    kiriview::ImageIoJob watchJob = repository.watchCandidateChanges(&receiver,
        ImageDocumentPageCandidateListSource::forDirectory(directoryUrl),
        [&changedCandidates](std::vector<ImageDocumentPageCandidate> candidates) {
            changedCandidates = std::move(candidates);
        },
        {});

    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 1);
    fakeProvider.emitDirectoryImageChanges(directoryUrl, { imageDocumentPageCandidate(imageUrl) });
    compareSingleImageDocumentPageCandidate(changedCandidates, imageUrl);

    watchJob.cancel();
    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 0);
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateRepository)

#include "tst_imagedocumentpagecandidaterepository.moc"
