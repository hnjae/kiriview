// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ARCHIVEFORMAT_H
#define KIRIVIEW_ARCHIVEFORMAT_H

#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

namespace kiriview {
enum class ArchiveStorageBackend {
    None,
    KZip,
    KTar,
    K7Zip,
    LibArchive,
};

enum class ArchiveOpenMatchKind {
    ComicBook,
    GeneralArchive,
};

struct ArchiveOpenMatch
{
    QString scheme;
    ArchiveOpenMatchKind kind = ArchiveOpenMatchKind::GeneralArchive;
};

ArchiveStorageBackend archiveStorageBackendForRootScheme(const QString& scheme);
bool archiveRootSchemeUsesKioFuse(const QString& scheme);
QStringList supportedComicBookArchiveExtensions();
bool isComicBookArchiveFileName(const QString& name);
std::optional<ArchiveOpenMatch> comicBookArchiveMatchForFileName(const QString& fileName);
std::optional<ArchiveOpenMatch> directArchiveOpenMatchForFileName(const QString& fileName);
std::optional<ArchiveOpenMatch> directArchiveOpenMatchForMimeTypeName(const QString& mimeTypeName);
std::optional<ArchiveOpenMatch> directArchiveOpenMatchForUrl(const QUrl& url);
}

#endif
