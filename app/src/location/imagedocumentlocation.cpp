// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagedocumentlocation.h"

#include "archive/archiveformat.h"
#include "bridge/rustqtconversion.h"
#include "kiriview/src/policy/archivepath.cxx.h"
#include "location/imageurl.h"

#include <QByteArray>
#include <optional>

namespace {
struct ArchiveCollectionRoot
{
    QUrl rootUrl;
    kiriview::OpenedCollectionScopeKind kind = kiriview::OpenedCollectionScopeKind::GeneralArchive;
};

struct UrlParts
{
    QByteArray scheme;
    QByteArray path;
    bool empty = true;
};

UrlParts urlParts(const QUrl& url)
{
    return UrlParts { url.scheme().toUtf8(), url.path().toUtf8(), url.isEmpty() };
}

std::optional<QUrl> archiveRootUrlForLocalArchive(const QUrl& url, const QString& archiveScheme)
{
    const QByteArray archiveSchemeBytes = archiveScheme.toUtf8();
    const QByteArray localPathBytes = url.toLocalFile().toUtf8();
    const kiriview::RustArchiveRootPath rootPath = kiriview::rustArchiveRootPathForLocalArchive(
        url.isLocalFile(), kiriview::Bridge::rustStr(archiveSchemeBytes),
        kiriview::Bridge::rustStr(localPathBytes));
    if (!rootPath.found) {
        return std::nullopt;
    }

    QUrl archiveRootUrl;
    archiveRootUrl.setScheme(archiveScheme);
    archiveRootUrl.setPath(kiriview::Bridge::qtString(rootPath.path));
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

bool openedCollectionScopeContainsUrlInRust(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& url)
{
    const UrlParts root = urlParts(openedCollectionScope.rootUrl());
    const UrlParts candidate = urlParts(url);
    return kiriview::rustOpenedCollectionScopeContainsUrl(openedCollectionScope.isEmpty(),
        root.empty, kiriview::Bridge::rustStr(root.scheme), kiriview::Bridge::rustStr(root.path),
        candidate.empty, kiriview::Bridge::rustStr(candidate.scheme),
        kiriview::Bridge::rustStr(candidate.path));
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
    return openedCollectionScopeContainsUrlInRust(openedCollectionScope, url);
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
