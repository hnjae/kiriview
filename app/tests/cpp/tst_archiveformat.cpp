// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/archiveformat.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <optional>

class TestArchiveFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void comicBookArchiveFileNamesAreCaseInsensitive();
    void directArchiveOpenMatchesExposeArchiveKind();
    void advertisedComicMimeTypesAreRoutableAndExcludeGeneralArchives();
    void archiveRootSchemesReportKioFuseSupportByBackend();
};

void TestArchiveFormat::comicBookArchiveFileNamesAreCaseInsensitive()
{
    QVERIFY(kiriview::isComicBookArchiveFileName(QStringLiteral("book.CBZ")));
    QVERIFY(kiriview::isComicBookArchiveFileName(QStringLiteral("book.CBT")));
    QVERIFY(kiriview::isComicBookArchiveFileName(QStringLiteral("book.CB7")));
    QVERIFY(kiriview::isComicBookArchiveFileName(QStringLiteral("book.CBR")));
    QVERIFY(!kiriview::isComicBookArchiveFileName(QStringLiteral("book.zip")));
    QVERIFY(!kiriview::isComicBookArchiveFileName(QStringLiteral("book.rar")));
}

void TestArchiveFormat::directArchiveOpenMatchesExposeArchiveKind()
{
    const std::optional<kiriview::ArchiveOpenMatch> cbzFileNameMatch
        = kiriview::directArchiveOpenMatchForFileName(QStringLiteral("book.cbz"));
    QVERIFY(cbzFileNameMatch.has_value());
    QCOMPARE(cbzFileNameMatch->scheme, QStringLiteral("zip"));
    QVERIFY(cbzFileNameMatch->kind == kiriview::ArchiveOpenMatchKind::ComicBook);

    const std::optional<kiriview::ArchiveOpenMatch> zipFileNameMatch
        = kiriview::directArchiveOpenMatchForFileName(QStringLiteral("book.zip"));
    QVERIFY(zipFileNameMatch.has_value());
    QCOMPARE(zipFileNameMatch->scheme, QStringLiteral("zip"));
    QVERIFY(zipFileNameMatch->kind == kiriview::ArchiveOpenMatchKind::GeneralArchive);

    const std::optional<kiriview::ArchiveOpenMatch> cbrMimeMatch
        = kiriview::directArchiveOpenMatchForMimeTypeName(QStringLiteral("application/x-cbr"));
    QVERIFY(cbrMimeMatch.has_value());
    QCOMPARE(cbrMimeMatch->scheme, QStringLiteral("rar"));
    QVERIFY(cbrMimeMatch->kind == kiriview::ArchiveOpenMatchKind::ComicBook);

    const std::optional<kiriview::ArchiveOpenMatch> rarMimeMatch
        = kiriview::directArchiveOpenMatchForMimeTypeName(QStringLiteral("application/vnd.rar"));
    QVERIFY(rarMimeMatch.has_value());
    QCOMPARE(rarMimeMatch->scheme, QStringLiteral("rar"));
    QVERIFY(rarMimeMatch->kind == kiriview::ArchiveOpenMatchKind::GeneralArchive);

    const std::optional<kiriview::ArchiveOpenMatch> cbzUrlMatch
        = kiriview::directArchiveOpenMatchForUrl(
            QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")));
    QVERIFY(cbzUrlMatch.has_value());
    QCOMPARE(cbzUrlMatch->scheme, QStringLiteral("zip"));
    QVERIFY(cbzUrlMatch->kind == kiriview::ArchiveOpenMatchKind::ComicBook);

    const std::optional<kiriview::ArchiveOpenMatch> rarUrlMatch
        = kiriview::directArchiveOpenMatchForUrl(
            QUrl::fromLocalFile(QStringLiteral("/books/book.rar")));
    QVERIFY(rarUrlMatch.has_value());
    QCOMPARE(rarUrlMatch->scheme, QStringLiteral("rar"));
    QVERIFY(rarUrlMatch->kind == kiriview::ArchiveOpenMatchKind::GeneralArchive);
}

void TestArchiveFormat::advertisedComicMimeTypesAreRoutableAndExcludeGeneralArchives()
{
    const QStringList mimeTypes = kiriview::supportedComicBookArchiveMimeTypes();
    QVERIFY(mimeTypes.contains(QStringLiteral("application/vnd.comicbook+zip")));
    QVERIFY(mimeTypes.contains(QStringLiteral("application/x-cbt")));
    QVERIFY(mimeTypes.contains(QStringLiteral("application/x-cb7")));
    QVERIFY(mimeTypes.contains(QStringLiteral("application/x-cbr")));
    QVERIFY(!mimeTypes.contains(QStringLiteral("application/zip")));
    QVERIFY(!mimeTypes.contains(QStringLiteral("application/vnd.rar")));

    QStringList sortedMimeTypes = mimeTypes;
    sortedMimeTypes.sort();
    sortedMimeTypes.removeDuplicates();
    QCOMPARE(mimeTypes, sortedMimeTypes);

    for (const QString& mimeType : mimeTypes) {
        const std::optional<kiriview::ArchiveOpenMatch> match
            = kiriview::directArchiveOpenMatchForMimeTypeName(mimeType);
        QVERIFY2(match.has_value(), qPrintable(mimeType));
        QVERIFY(match->kind == kiriview::ArchiveOpenMatchKind::ComicBook);
        QVERIFY(kiriview::archiveStorageBackendForRootScheme(match->scheme)
            != kiriview::ArchiveStorageBackend::None);
    }
}

void TestArchiveFormat::archiveRootSchemesReportKioFuseSupportByBackend()
{
    QVERIFY(kiriview::archiveRootSchemeUsesKioFuse(QStringLiteral("zip")));
    QVERIFY(kiriview::archiveRootSchemeUsesKioFuse(QStringLiteral("tar")));
    QVERIFY(kiriview::archiveRootSchemeUsesKioFuse(QStringLiteral("sevenz")));
    QVERIFY(!kiriview::archiveRootSchemeUsesKioFuse(QStringLiteral("rar")));
    QVERIFY(!kiriview::archiveRootSchemeUsesKioFuse(QStringLiteral("unknown")));
}

QTEST_GUILESS_MAIN(TestArchiveFormat)

#include "tst_archiveformat.moc"
