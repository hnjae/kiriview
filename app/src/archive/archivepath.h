// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ARCHIVEPATH_H
#define KIRIVIEW_ARCHIVEPATH_H

#include <QString>
#include <QUrl>

namespace kiriview {
class OpenedCollectionScopeLocation;

QString normalizedArchiveRootPath(const QUrl& archiveRootUrl);
QString normalizedArchiveEntryPath(const QString& entryPath);
QString archiveRelativePathForUrl(const QUrl& archiveRootUrl, const QUrl& url);
QString openedCollectionEntryPathForUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl);
QUrl openedCollectionEntryUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath);
}

#endif
