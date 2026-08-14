// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archivepath.h"

#include "location/imagelocation.h"

#include <QDir>

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

}
