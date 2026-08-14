// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagelocation.h"

#include "archive/archivepath.h"
#include "location/imageurl.h"

#include <QDir>

namespace {
kiriview::SourceKey openedCollectionBackingKey(const kiriview::OpenedCollectionScopeLocation& scope)
{
    return kiriview::sourceKeyForUrl(scope.fileUrl());
}

kiriview::SourceKey directoryCollectionKey(const QUrl& url)
{
    return kiriview::sourceKeyForUrl(kiriview::normalizedFileContainerUrl(url));
}

QString openedCollectionRootIdentity(const kiriview::OpenedCollectionScopeLocation& scope)
{
    const kiriview::SourceKey backingKey = openedCollectionBackingKey(scope);
    if (!backingKey.valid) {
        return {};
    }
    if (scope.kind() == kiriview::OpenedCollectionScopeKind::Directory) {
        const kiriview::SourceKey rootKey = directoryCollectionKey(scope.rootUrl());
        return rootKey.valid ? QStringLiteral("directory\x1f%1").arg(rootKey.identity) : QString();
    }

    const QUrl& requestedUrl = scope.fileUrl();
    QString expectedRootPath;
    if (requestedUrl.isLocalFile()) {
        expectedRootPath = QDir::cleanPath(requestedUrl.toLocalFile());
        if (!expectedRootPath.endsWith(QLatin1Char('/'))) {
            expectedRootPath += QLatin1Char('/');
        }
    }
    const bool rootDerivedFromBacking = !expectedRootPath.isEmpty()
        && scope.rootUrl().path() == expectedRootPath && scope.rootUrl().query().isEmpty()
        && scope.rootUrl().fragment().isEmpty() && !scope.rootUrl().scheme().isEmpty();
    if (rootDerivedFromBacking) {
        return QStringLiteral("archive\x1f%1\x1f%2")
            .arg(scope.rootUrl().scheme(), backingKey.identity);
    }

    const kiriview::SourceKey rawRootKey = kiriview::sourceKeyForUrl(scope.rootUrl());
    return rawRootKey.valid ? QStringLiteral("raw\x1f%1").arg(rawRootKey.identity) : QString();
}

QString openedCollectionEntryIdentity(
    const kiriview::OpenedCollectionScopeLocation& scope, const QUrl& imageUrl)
{
    if (scope.kind() != kiriview::OpenedCollectionScopeKind::Directory) {
        const QString entryPath = kiriview::openedCollectionEntryPathForUrl(scope, imageUrl);
        if (!entryPath.isEmpty()) {
            return QStringLiteral("entry\x1f%1").arg(entryPath);
        }
    }

    const kiriview::SourceKey imageKey = kiriview::sourceKeyForUrl(imageUrl);
    return imageKey.valid ? QStringLiteral("source\x1f%1").arg(imageKey.identity) : QString();
}
}

namespace kiriview {
std::optional<DirectMediaPageScopeIdentity> directMediaPageScopeIdentityForSource(
    const ResolvedNavigationSource& source)
{
    if (source.isEmpty()) {
        return std::nullopt;
    }

    SourceKey currentKey = sourceKeyForUrl(source.navigationUrl());
    if (!currentKey.valid) {
        return std::nullopt;
    }

    SourceKey parentKey
        = sourceKeyForUrl(parentDirectoryUrlForFileNavigation(currentKey.normalizedUrl));
    if (!parentKey.valid) {
        return std::nullopt;
    }

    return DirectMediaPageScopeIdentity(std::move(currentKey), std::move(parentKey));
}

std::optional<DirectMediaPageScopeIdentity> directMediaPageScopeIdentityForOwnerCandidate(
    const QUrl& candidateUrl, const SourceKey& ownerParentKey)
{
    SourceKey currentKey = sourceKeyForUrl(candidateUrl);
    if (!currentKey.valid || !ownerParentKey.valid) {
        return std::nullopt;
    }

    const SourceKey candidateParentKey
        = sourceKeyForUrl(parentDirectoryUrlForFileNavigation(currentKey.normalizedUrl));
    if (!sameSourceKey(candidateParentKey, ownerParentKey)) {
        return std::nullopt;
    }

    return DirectMediaPageScopeIdentity(std::move(currentKey), ownerParentKey);
}

bool sameOpenedCollectionScopeLocation(
    const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
{
    if (left.isEmpty() || right.isEmpty()) {
        return left.isEmpty() && right.isEmpty();
    }

    return left.kind() == right.kind()
        && sameSourceKey(openedCollectionBackingKey(left), openedCollectionBackingKey(right))
        && openedCollectionRootIdentity(left) == openedCollectionRootIdentity(right);
}

bool sameOpenedCollectionEntryLocation(const OpenedCollectionScopeLocation& leftScope,
    const QUrl& leftUrl, const OpenedCollectionScopeLocation& rightScope, const QUrl& rightUrl)
{
    const QString leftIdentity = openedCollectionEntryIdentity(leftScope, leftUrl);
    return !leftIdentity.isEmpty()
        && leftIdentity == openedCollectionEntryIdentity(rightScope, rightUrl);
}

bool sameOpenedCollectionScopeSnapshot(
    const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
{
    return sameOpenedCollectionScopeLocation(left, right)
        && sameResolvedNavigationSourceSnapshot(left.source(), right.source());
}

QString displayScopeIdentityForLocation(const DisplayedImageLocation& location)
{
    if (location.isEmpty()) {
        return {};
    }

    const OpenedCollectionScopeLocation& openedCollectionScope = location.openedCollectionScope();
    if (!openedCollectionScope.isEmpty()) {
        const QString backingIdentity = openedCollectionBackingKey(openedCollectionScope).identity;
        const QString rootIdentity = openedCollectionRootIdentity(openedCollectionScope);
        const QString imageIdentity
            = openedCollectionEntryIdentity(openedCollectionScope, location.imageUrl());
        return QStringLiteral("opened\x1f%1\x1f%2\x1f%3\x1f%4")
            .arg(static_cast<int>(openedCollectionScope.kind()))
            .arg(backingIdentity, rootIdentity, imageIdentity);
    }

    const std::optional<DirectMediaPageScopeIdentity>& directIdentity
        = location.directMediaPageScopeIdentity();
    if (!directIdentity.has_value()) {
        return {};
    }

    return QStringLiteral("direct\x1f%1\x1f%2")
        .arg(directIdentity->currentKey().identity, directIdentity->parentKey().identity);
}
}
