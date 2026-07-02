// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionthumbnailruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <utility>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

QUrl archivePageUrl(const QUrl& rootUrl, const QString& entryPath)
{
    QUrl url = rootUrl;
    url.setPath(rootUrl.path() + entryPath);
    return url;
}

kiriview::ActiveNavigationThumbnailRow thumbnailRow(int number, const QUrl& url)
{
    return kiriview::ActiveNavigationThumbnailRow {
        number,
        url,
        url.fileName(QUrl::PrettyDecoded),
        kiriview::ActiveNavigationThumbnailKind::Image,
        kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageImage,
        true,
    };
}

}

class TestDocumentSessionThumbnailRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void imageDocumentPageRowsUseOpenedCollectionScopeFromLeafSnapshot();
    void directoryCollectionRowsStayPlaceholderOnly();
    void nonZipArchiveCollectionRowsStayPlaceholderOnly();
};

void TestDocumentSessionThumbnailRuntime::
    imageDocumentPageRowsUseOpenedCollectionScopeFromLeafSnapshot()
{
    using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;
    QObject owner;
    const kiriview::OpenedCollectionScopeLocation cbzScope
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            localUrl(QStringLiteral("/books/book.cbz")),
            QUrl(QStringLiteral("zip:///books/book.cbz/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.displayedOpenedCollectionScope = cbzScope;

    kiriview::DocumentSessionImageDocumentSnapshotPort imageDocument;
    imageDocument.snapshot = [&imageSnapshot]() { return imageSnapshot; };

    kiriview::ThumbnailGenerationRequest generatedRequest;
    int generationCount = 0;
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies;
    dependencies.generationProvider
        = [&generatedRequest, &generationCount](QObject*,
              kiriview::ThumbnailGenerationRequest request, kiriview::ThumbnailGenerationCallback) {
              ++generationCount;
              generatedRequest = std::move(request);
              return kiriview::ImageIoJob {};
          };
    dependencies.lookupProvider
        = [](QObject*, kiriview::ThumbnailCacheLookupRequest,
              kiriview::ThumbnailCacheLookupCallback) { return kiriview::ImageIoJob {}; };

    kiriview::DocumentSessionThumbnailRuntime runtime(
        &owner, &imageDocument, std::move(dependencies));
    const QUrl pageUrl = archivePageUrl(cbzScope.rootUrl(), QStringLiteral("pages/001.png"));
    runtime.setRows({ thumbnailRow(1, pageUrl) });

    QVERIFY(
        runtime.reportDemand(1, pageUrl, 256, Priority::Visible, runtime.navigationGeneration()));

    QCOMPARE(generationCount, 1);
    QCOMPARE(generatedRequest.openedCollectionScope, cbzScope);
    QCOMPARE(generatedRequest.sourceUrl, pageUrl);
    QCOMPARE(generatedRequest.cacheInstallEnabled, true);
}

void TestDocumentSessionThumbnailRuntime::directoryCollectionRowsStayPlaceholderOnly()
{
    using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;
    QObject owner;
    const kiriview::OpenedCollectionScopeLocation directoryScope
        = kiriview::OpenedCollectionScopeLocation::fromUrls(localUrl(QStringLiteral("/books/book")),
            localUrl(QStringLiteral("/books/book/")),
            kiriview::OpenedCollectionScopeKind::Directory);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.displayedOpenedCollectionScope = directoryScope;

    kiriview::DocumentSessionImageDocumentSnapshotPort imageDocument;
    imageDocument.snapshot = [&imageSnapshot]() { return imageSnapshot; };

    int lookupCount = 0;
    int generationCount = 0;
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies;
    dependencies.lookupProvider = [&lookupCount](QObject*, kiriview::ThumbnailCacheLookupRequest,
                                      kiriview::ThumbnailCacheLookupCallback) {
        ++lookupCount;
        return kiriview::ImageIoJob {};
    };
    dependencies.generationProvider
        = [&generationCount](QObject*, kiriview::ThumbnailGenerationRequest,
              kiriview::ThumbnailGenerationCallback) {
              ++generationCount;
              return kiriview::ImageIoJob {};
          };

    kiriview::DocumentSessionThumbnailRuntime runtime(
        &owner, &imageDocument, std::move(dependencies));
    const QUrl pageUrl = localUrl(QStringLiteral("/books/book/chapter/001.png"));
    runtime.setRows({ thumbnailRow(1, pageUrl) });

    QVERIFY(
        runtime.reportDemand(1, pageUrl, 256, Priority::Visible, runtime.navigationGeneration()));

    QCOMPARE(lookupCount, 0);
    QCOMPARE(generationCount, 0);
}

void TestDocumentSessionThumbnailRuntime::nonZipArchiveCollectionRowsStayPlaceholderOnly()
{
    using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;
    QObject owner;
    const kiriview::OpenedCollectionScopeLocation cb7Scope
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            localUrl(QStringLiteral("/books/book.cb7")),
            QUrl(QStringLiteral("sevenz:///books/book.cb7/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.displayedOpenedCollectionScope = cb7Scope;

    kiriview::DocumentSessionImageDocumentSnapshotPort imageDocument;
    imageDocument.snapshot = [&imageSnapshot]() { return imageSnapshot; };

    int lookupCount = 0;
    int generationCount = 0;
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies;
    dependencies.lookupProvider = [&lookupCount](QObject*, kiriview::ThumbnailCacheLookupRequest,
                                      kiriview::ThumbnailCacheLookupCallback) {
        ++lookupCount;
        return kiriview::ImageIoJob {};
    };
    dependencies.generationProvider
        = [&generationCount](QObject*, kiriview::ThumbnailGenerationRequest,
              kiriview::ThumbnailGenerationCallback) {
              ++generationCount;
              return kiriview::ImageIoJob {};
          };

    kiriview::DocumentSessionThumbnailRuntime runtime(
        &owner, &imageDocument, std::move(dependencies));
    const QUrl pageUrl = archivePageUrl(cb7Scope.rootUrl(), QStringLiteral("pages/001.png"));
    runtime.setRows({ thumbnailRow(1, pageUrl) });

    QVERIFY(
        runtime.reportDemand(1, pageUrl, 256, Priority::Visible, runtime.navigationGeneration()));

    QCOMPARE(lookupCount, 0);
    QCOMPARE(generationCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionThumbnailRuntime)

#include "test_documentsessionthumbnailruntime.moc"
