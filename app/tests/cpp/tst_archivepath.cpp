// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/archivepath.h"
#include "location/imagelocation.h"

#include <QObject>
#include <QString>
#include <QTest>
#include <QUrl>

namespace {
kiriview::OpenedCollectionScopeLocation openedCollectionScope()
{
    return kiriview::OpenedCollectionScopeLocation::fromUrls(
        QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")),
        QUrl(QStringLiteral("zip:///books/book.cbz/")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
}
}

class TestArchivePath : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void entryUrlsNormalizePathsAndClearUrlMetadata();
    void entryUrlsRejectUnsafePaths();
    void entryPathsResolveOnlyInsideArchiveRoot();
};

void TestArchivePath::entryUrlsNormalizePathsAndClearUrlMetadata()
{
    kiriview::OpenedCollectionScopeLocation archive = openedCollectionScope();
    QUrl rootUrl = archive.rootUrl();
    rootUrl.setQuery(QStringLiteral("token=ignored"));
    rootUrl.setFragment(QStringLiteral("ignored"));
    archive = kiriview::OpenedCollectionScopeLocation::fromUrls(
        archive.fileUrl(), rootUrl, archive.kind());

    const QUrl url
        = kiriview::openedCollectionEntryUrl(archive, QStringLiteral("./chapter/./page001.png"));

    QCOMPARE(url, QUrl(QStringLiteral("zip:///books/book.cbz/chapter/page001.png")));
    QVERIFY(url.query().isEmpty());
    QVERIFY(url.fragment().isEmpty());
}

void TestArchivePath::entryUrlsRejectUnsafePaths()
{
    const kiriview::OpenedCollectionScopeLocation archive = openedCollectionScope();

    QVERIFY(
        kiriview::openedCollectionEntryUrl(archive, QStringLiteral("../page001.png")).isEmpty());
    QVERIFY(
        kiriview::openedCollectionEntryUrl(archive, QStringLiteral("/tmp/page001.png")).isEmpty());
    QVERIFY(kiriview::openedCollectionEntryUrl(
        kiriview::OpenedCollectionScopeLocation::none(), QStringLiteral("page001.png"))
            .isEmpty());
}

void TestArchivePath::entryPathsResolveOnlyInsideArchiveRoot()
{
    const kiriview::OpenedCollectionScopeLocation archive = openedCollectionScope();

    QCOMPARE(kiriview::openedCollectionEntryPathForUrl(
                 archive, QUrl(QStringLiteral("zip:///books/book.cbz/chapter/page001.png"))),
        QStringLiteral("chapter/page001.png"));
    QVERIFY(kiriview::openedCollectionEntryPathForUrl(
        archive, QUrl(QStringLiteral("zip:///books/book.cbz/")))
            .isEmpty());
    QVERIFY(kiriview::openedCollectionEntryPathForUrl(
        archive, QUrl(QStringLiteral("zip:///books/book.cbz/../page001.png")))
            .isEmpty());
    QVERIFY(kiriview::openedCollectionEntryPathForUrl(
        archive, QUrl(QStringLiteral("tar:///books/book.cbz/chapter/page001.png")))
            .isEmpty());
}

QTEST_GUILESS_MAIN(TestArchivePath)

#include "tst_archivepath.moc"
