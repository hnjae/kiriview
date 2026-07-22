// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagedocumentlocation.h"

#include "archive/archiveformat.h"
#include "archive/archivepath.h"
#include "location/imageurl.h"

#include <QDir>
#include <optional>

namespace {
struct ArchiveCollectionRoot
{
    QUrl rootUrl;
    kiriview::OpenedCollectionScopeKind kind = kiriview::OpenedCollectionScopeKind::GeneralArchive;
};

std::optional<QUrl> archiveRootUrlForLocalArchive(const QUrl& url, const QString& archiveScheme)
{
    if (!url.isLocalFile() || archiveScheme.isEmpty()) {
        return std::nullopt;
    }
    const QString localPath = QDir::cleanPath(url.toLocalFile());
    if (localPath.isEmpty()) {
        return std::nullopt;
    }

    QUrl archiveRootUrl;
    archiveRootUrl.setScheme(archiveScheme);
    QUrl pathUrl;
    pathUrl.setPath(localPath);
    archiveRootUrl.setPath(kiriview::normalizedArchiveRootPath(pathUrl));
    if (!archiveRootUrl.isValid() || archiveRootUrl.path().isEmpty()) {
        return std::nullopt;
    }

    return archiveRootUrl;
}

kiriview::OpenedCollectionScopeKind archiveCollectionKindForMatch(
    const kiriview::ArchiveOpenMatch& match)
{
    switch (match.kind) {
    case kiriview::ArchiveOpenMatchKind::ComicBook:
        return kiriview::OpenedCollectionScopeKind::ComicBookArchive;
    case kiriview::ArchiveOpenMatchKind::GeneralArchive:
        return kiriview::OpenedCollectionScopeKind::GeneralArchive;
    }

    return kiriview::OpenedCollectionScopeKind::GeneralArchive;
}

std::optional<ArchiveCollectionRoot> archiveCollectionRootForLocalArchive(
    const QUrl& url, std::optional<kiriview::ArchiveOpenMatch> match)
{
    if (!match.has_value()) {
        return std::nullopt;
    }

    const std::optional<QUrl> rootUrl = archiveRootUrlForLocalArchive(url, match->scheme);
    if (!rootUrl.has_value()) {
        return std::nullopt;
    }

    return ArchiveCollectionRoot { *rootUrl, archiveCollectionKindForMatch(*match) };
}

std::optional<ArchiveCollectionRoot> directArchiveCollectionRootForLocalArchive(const QUrl& url)
{
    return archiveCollectionRootForLocalArchive(url, kiriview::directArchiveOpenMatchForUrl(url));
}

std::optional<kiriview::OpenedCollectionScopeLocation>
directoryOpenedCollectionScopeLocationForLocalSource(
    const kiriview::ResolvedNavigationSource& source)
{
    const QUrl& url = source.requestedUrl();
    if (source.entryKind() != kiriview::NavigationSourceEntryKind::Directory
        || !url.isLocalFile()) {
        return std::nullopt;
    }

    const QUrl fileUrl = kiriview::normalizedFileContainerUrl(url);
    if (fileUrl.toLocalFile().isEmpty()) {
        return std::nullopt;
    }

    return kiriview::OpenedCollectionScopeLocation::fromResolvedSource(source,
        kiriview::normalizedDirectoryContainerUrl(fileUrl),
        kiriview::OpenedCollectionScopeKind::Directory);
}

QUrl openedCollectionScopeSourceNavigationUrl(const kiriview::DisplayedImageLocation& location)
{
    return kiriview::normalizedFileContainerUrl(
        location.openedCollectionScope().navigationSourceUrl());
}

}

namespace kiriview {
std::optional<OpenedCollectionScopeLocation> openedCollectionScopeLocationForLocalArchiveSource(
    const ResolvedNavigationSource& source)
{
    const std::optional<ArchiveCollectionRoot> root
        = directArchiveCollectionRootForLocalArchive(source.requestedUrl());
    if (!root.has_value()) {
        return std::nullopt;
    }

    return OpenedCollectionScopeLocation::fromResolvedSource(source, root->rootUrl, root->kind);
}

std::optional<OpenedCollectionScopeLocation> openedCollectionScopeLocationForResolvedExternalSource(
    const ResolvedNavigationSource& source)
{
    switch (source.entryKind()) {
    case NavigationSourceEntryKind::Direct:
        return std::nullopt;
    case NavigationSourceEntryKind::Directory:
        return directoryOpenedCollectionScopeLocationForLocalSource(source);
    case NavigationSourceEntryKind::Archive:
        return openedCollectionScopeLocationForLocalArchiveSource(source);
    }
    return std::nullopt;
}

bool openedCollectionScopeContainsUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& url)
{
    return !openedCollectionScope.isEmpty()
        && !archiveRelativePathForUrl(openedCollectionScope.rootUrl(), url).isEmpty();
}

bool displayedLocationIsInsideOpenedCollectionScope(const DisplayedImageLocation& location)
{
    return openedCollectionScopeContainsUrl(location.openedCollectionScope(), location.imageUrl());
}

QString windowTitleFileNameForDisplayedLocation(const DisplayedImageLocation& location)
{
    if (location.imageUrl().isEmpty()) {
        return QString();
    }

    if (displayedLocationIsInsideOpenedCollectionScope(location)
        && !location.openedCollectionScopeSourceUrl().fileName().isEmpty()) {
        return location.openedCollectionScopeSourceUrl().fileName();
    }

    return location.imageUrl().fileName();
}

QUrl containerNavigationUrlForLocation(const DisplayedImageLocation& location)
{
    if (!location.openedCollectionScope().isComicBook()
        || !displayedLocationIsInsideOpenedCollectionScope(location)) {
        return {};
    }

    return openedCollectionScopeSourceNavigationUrl(location);
}

}
