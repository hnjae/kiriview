// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archiveformat.h"

#include "bridge/archiveformatconversion.h"
#include "bridge/rustqtconversion.h"
#include "kiriview/src/policy/archiveformat.cxx.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <optional>

namespace {
using ArchiveMatchResolver = std::optional<kiriview::ArchiveOpenMatch> (*)(const QString&);

std::optional<kiriview::ArchiveOpenMatch> archiveMatchForQString(
    const QString& value, kiriview::RustArchiveOpenMatch (*rustFunction)(rust::Str))
{
    return kiriview::archiveOpenMatchFromBridge(
        kiriview::Bridge::rustResultForQString(value, rustFunction));
}

std::optional<kiriview::ArchiveOpenMatch> archiveMatchForUrl(const QUrl& url,
    QMimeDatabase::MatchMode mimeMatchMode, ArchiveMatchResolver matchForFileName,
    ArchiveMatchResolver matchForMimeTypeName)
{
    if (!url.isLocalFile()) {
        return std::nullopt;
    }

    std::optional<kiriview::ArchiveOpenMatch> extensionMatch = matchForFileName(url.fileName());
    if (extensionMatch.has_value()) {
        return extensionMatch;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(url.toLocalFile(), mimeMatchMode);
    return matchForMimeTypeName(mimeType.name());
}

}

namespace kiriview {
ArchiveStorageBackend archiveStorageBackendForRootScheme(const QString& scheme)
{
    return archiveStorageBackendFromBridge(
        Bridge::rustResultForQString(scheme, rustArchiveStorageBackendForRootScheme));
}

bool archiveRootSchemeUsesKioFuse(const QString& scheme)
{
    return Bridge::rustResultForQString(scheme, rustArchiveRootSchemeUsesKioFuse);
}

QStringList supportedComicBookArchiveExtensions()
{
    return Bridge::qtStringList(rustSupportedComicBookArchiveExtensions());
}

bool isComicBookArchiveFileName(const QString& name)
{
    return comicBookArchiveMatchForFileName(name).has_value();
}

std::optional<ArchiveOpenMatch> comicBookArchiveMatchForFileName(const QString& fileName)
{
    return archiveMatchForQString(fileName, rustComicBookArchiveMatchForFileName);
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForFileName(const QString& fileName)
{
    return archiveMatchForQString(fileName, rustDirectArchiveOpenMatchForFileName);
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForMimeTypeName(const QString& mimeTypeName)
{
    return archiveMatchForQString(mimeTypeName, rustDirectArchiveOpenMatchForMimeTypeName);
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForUrl(const QUrl& url)
{
    return archiveMatchForUrl(url, QMimeDatabase::MatchDefault,
        kiriview::directArchiveOpenMatchForFileName,
        kiriview::directArchiveOpenMatchForMimeTypeName);
}

}
