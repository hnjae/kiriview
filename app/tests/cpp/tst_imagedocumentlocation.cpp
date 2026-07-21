// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <optional>

namespace {
QString archivePageWindowTitle(const QUrl& pageUrl, const QUrl& archiveFileUrl,
    const QUrl& archiveRootUrl, kiriview::OpenedCollectionScopeKind kind)
{
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(archiveFileUrl, archiveRootUrl, kind);
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(pageUrl, archiveCollection);
    return kiriview::windowTitleFileNameForDisplayedLocation(location);
}

std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection(const QUrl& archiveUrl)
{
    return kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(archiveUrl, {}));
}
}

class TestImageDocumentLocation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void comicBookArchiveRootUrlsUseZipScheme();
    void comicBookArchiveRootUrlsUseFormatSpecificKioSchemes();
    void directArchiveRootUrlsUseFormatSpecificKioSchemes();
    void archiveCollectionPagesResolveToArchiveZoomScope();
    void directArchivePagesResolveToZoomScopeOnly();
    void directoryCollectionPagesResolveToDirectoryCollectionScope();
    void regularImagesDoNotResolveToZoomScopes();
    void explicitKdeArchiveUrlImagesDoNotResolveToZoomScopes();
};

void TestImageDocumentLocation::comicBookArchiveRootUrlsUseZipScheme()
{
    const QUrl archiveUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz"));
    const auto collection = archiveCollection(archiveUrl);

    QVERIFY(collection.has_value());
    QCOMPARE(collection->rootUrl().scheme(), QStringLiteral("zip"));
    QCOMPARE(collection->rootUrl().path(), QStringLiteral("/books/book.cbz/"));
}

void TestImageDocumentLocation::comicBookArchiveRootUrlsUseFormatSpecificKioSchemes()
{
    const auto cbt = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.cbt")));
    QVERIFY(cbt.has_value());
    QCOMPARE(cbt->rootUrl().scheme(), QStringLiteral("tar"));
    QCOMPARE(cbt->rootUrl().path(), QStringLiteral("/books/book.cbt/"));

    const auto cb7 = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.cb7")));
    QVERIFY(cb7.has_value());
    QCOMPARE(cb7->rootUrl().scheme(), QStringLiteral("sevenz"));
    QCOMPARE(cb7->rootUrl().path(), QStringLiteral("/books/book.cb7/"));

    const auto cbr = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.cbr")));
    QVERIFY(cbr.has_value());
    QCOMPARE(cbr->rootUrl().scheme(), QStringLiteral("rar"));
    QCOMPARE(cbr->rootUrl().path(), QStringLiteral("/books/book.cbr/"));

    QVERIFY(archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.zip")))->kind()
        == kiriview::OpenedCollectionScopeKind::GeneralArchive);
    QVERIFY(!archiveCollection(QUrl(QStringLiteral("smb://server/books/book.cbz"))).has_value());
}

void TestImageDocumentLocation::directArchiveRootUrlsUseFormatSpecificKioSchemes()
{
    const auto zip = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.zip")));
    QVERIFY(zip.has_value());
    QCOMPARE(zip->rootUrl().scheme(), QStringLiteral("zip"));
    QCOMPARE(zip->rootUrl().path(), QStringLiteral("/books/book.zip/"));

    const auto tar = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.tar")));
    QVERIFY(tar.has_value());
    QCOMPARE(tar->rootUrl().scheme(), QStringLiteral("tar"));
    QCOMPARE(tar->rootUrl().path(), QStringLiteral("/books/book.tar/"));

    const auto sevenZip = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.7z")));
    QVERIFY(sevenZip.has_value());
    QCOMPARE(sevenZip->rootUrl().scheme(), QStringLiteral("sevenz"));
    QCOMPARE(sevenZip->rootUrl().path(), QStringLiteral("/books/book.7z/"));

    const auto rar = archiveCollection(QUrl::fromLocalFile(QStringLiteral("/books/book.rar")));
    QVERIFY(rar.has_value());
    QCOMPARE(rar->rootUrl().scheme(), QStringLiteral("rar"));
    QCOMPARE(rar->rootUrl().path(), QStringLiteral("/books/book.rar/"));

    QVERIFY(!archiveCollection(QUrl(QStringLiteral("smb://server/books/book.zip"))).has_value());
}

void TestImageDocumentLocation::archiveCollectionPagesResolveToArchiveZoomScope()
{
    const QUrl archiveUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());

    QUrl pageUrl = archiveCollection->rootUrl();
    pageUrl.setPath(archiveCollection->rootUrl().path() + QStringLiteral("chapter/page001.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(pageUrl, *archiveCollection);

    QCOMPARE(kiriview::containerNavigationUrlForLocation(location), archiveUrl);
}

void TestImageDocumentLocation::directArchivePagesResolveToZoomScopeOnly()
{
    const QUrl archiveUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.zip"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());

    QUrl pageUrl = archiveCollection->rootUrl();
    pageUrl.setPath(archiveCollection->rootUrl().path() + QStringLiteral("chapter/page001.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(pageUrl, *archiveCollection);

    QVERIFY(kiriview::containerNavigationUrlForLocation(location).isEmpty());
}

void TestImageDocumentLocation::directoryCollectionPagesResolveToDirectoryCollectionScope()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QUrl directoryUrl = QUrl::fromLocalFile(directory.path());
    const std::optional<kiriview::OpenedCollectionScopeLocation> directoryCollection
        = kiriview::openedCollectionScopeLocationForResolvedExternalSource(
            kiriview::NavigationSourceResolver().resolveExternalSource(directoryUrl));
    QVERIFY(directoryCollection.has_value());
    QCOMPARE(directoryCollection->kind(), kiriview::OpenedCollectionScopeKind::Directory);
    QCOMPARE(directoryCollection->fileUrl(), directoryUrl);
    QCOMPARE(directoryCollection->rootUrl().scheme(), QStringLiteral("file"));
    QVERIFY(directoryCollection->rootUrl().path().endsWith(QLatin1Char('/')));

    QUrl pageUrl = directoryCollection->rootUrl();
    pageUrl.setPath(directoryCollection->rootUrl().path() + QStringLiteral("chapter/page001.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            pageUrl, *directoryCollection);

    QVERIFY(kiriview::containerNavigationUrlForLocation(location).isEmpty());
    QCOMPARE(kiriview::windowTitleFileNameForDisplayedLocation(location), directoryUrl.fileName());
}

void TestImageDocumentLocation::regularImagesDoNotResolveToZoomScopes()
{
    const QUrl fileUrl = QUrl::fromLocalFile(QStringLiteral("/images/page.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(fileUrl);

    QVERIFY(kiriview::containerNavigationUrlForLocation(location).isEmpty());
}

void TestImageDocumentLocation::explicitKdeArchiveUrlImagesDoNotResolveToZoomScopes()
{
    const kiriview::DisplayedImageLocation location = kiriview::DisplayedImageLocation::fromUrl(
        QUrl(QStringLiteral("zip:///books/book.cbz/chapter/page001.png")));

    QVERIFY(kiriview::containerNavigationUrlForLocation(location).isEmpty());
}

QTEST_GUILESS_MAIN(TestImageDocumentLocation)

#include "tst_imagedocumentlocation.moc"
