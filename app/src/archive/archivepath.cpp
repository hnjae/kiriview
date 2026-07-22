// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archivepath.h"

#include "archive/archiveformat.h"
#include "location/imagelocation.h"

#include <QDir>
#include <optional>

namespace {
QString cleanPath(const QString& path) { return QDir::cleanPath(path); }
}

namespace kiriview {
QString normalizedArchiveRootPath(const QUrl& archiveRootUrl)
{
    QString path = cleanPath(archiveRootUrl.path());
    if (!path.endsWith(u'/')) {
        path.append(u'/');
    }
    return path;
}

QString normalizedArchiveEntryPath(const QString& entryPath)
{
    QString path = cleanPath(entryPath);
    while (path.startsWith(QStringLiteral("./"))) {
        path.remove(0, 2);
    }
    if (path == QStringLiteral(".") || path == QStringLiteral("..")
        || path.startsWith(QStringLiteral("../")) || path.startsWith(u'/')) {
        return {};
    }
    return path;
}

QString archiveRelativePathForUrl(const QUrl& archiveRootUrl, const QUrl& url)
{
    if (archiveRootUrl.isEmpty() || url.isEmpty() || archiveRootUrl.scheme() != url.scheme()) {
        return {};
    }
    const QString rootPath = normalizedArchiveRootPath(archiveRootUrl);
    const QString path = cleanPath(url.path());
    if (path.size() <= rootPath.size() || !path.startsWith(rootPath)) {
        return {};
    }
    return path.sliced(rootPath.size());
}

QString openedCollectionEntryPathForUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl)
{
    if (openedCollectionScope.isEmpty()) {
        return {};
    }

    return normalizedArchiveEntryPath(
        archiveRelativePathForUrl(openedCollectionScope.rootUrl(), imageUrl));
}

QUrl openedCollectionEntryUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath)
{
    const QString cleanEntryPath = normalizedArchiveEntryPath(entryPath);
    if (openedCollectionScope.isEmpty() || cleanEntryPath.isEmpty()) {
        return {};
    }

    QUrl url = openedCollectionScope.rootUrl();
    url.setPath(normalizedArchiveRootPath(url) + cleanEntryPath);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

std::optional<KioFuseArchivePath> kioFuseArchivePath(
    const QString& localPath, const QString& runtimeDir)
{
    constexpr QStringView marker = u"/kio-fuse-";
    const QString path = cleanPath(localPath);
    qsizetype markerIndex = -1;
    if (runtimeDir.isEmpty()) {
        markerIndex = path.indexOf(marker);
    } else {
        const QString prefix = cleanPath(runtimeDir) + marker;
        if (path.startsWith(prefix)) {
            markerIndex = prefix.size() - marker.size();
        }
    }
    if (markerIndex < 0) {
        return std::nullopt;
    }
    const qsizetype mountNameStart = markerIndex + marker.size();
    const qsizetype mountEnd = path.indexOf(u'/', mountNameStart);
    if (mountEnd < 0 || mountEnd == path.size() - 1) {
        return std::nullopt;
    }
    const QString relativePath = path.sliced(mountEnd + 1);
    const qsizetype schemeEnd = relativePath.indexOf(u'/');
    if (schemeEnd <= 0 || schemeEnd == relativePath.size() - 1) {
        return std::nullopt;
    }
    const QString scheme = relativePath.first(schemeEnd);
    if (!archiveRootSchemeUsesKioFuse(scheme)) {
        return std::nullopt;
    }
    return KioFuseArchivePath { scheme, relativePath.sliced(schemeEnd) };
}

std::optional<QUrl> kioFuseArchiveUrlForLocalPath(
    const QString& localPath, const QString& runtimeDir)
{
    const std::optional<KioFuseArchivePath> archivePath = kioFuseArchivePath(localPath, runtimeDir);
    if (!archivePath.has_value()) {
        return std::nullopt;
    }

    QUrl url;
    url.setScheme(archivePath->scheme);
    url.setPath(archivePath->path);
    if (!url.isValid() || url.path().isEmpty()) {
        return std::nullopt;
    }

    return url;
}
}
