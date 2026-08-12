// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagelocation.h"

#include "location/imageurl.h"

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
    return sameNormalizedUrl(left.fileUrl(), right.fileUrl())
        && sameNormalizedUrl(left.rootUrl(), right.rootUrl()) && left.kind() == right.kind();
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
        const QString scopeFileIdentity
            = normalizedUrlIdentityKey(openedCollectionScope.fileUrl(), QUrl::FullyEncoded);
        const QString scopeRootIdentity
            = normalizedUrlIdentityKey(openedCollectionScope.rootUrl(), QUrl::FullyEncoded);
        const QString imageIdentity
            = normalizedUrlIdentityKey(location.imageUrl(), QUrl::FullyEncoded);
        return QStringLiteral("opened\x1f%1\x1f%2\x1f%3\x1f%4")
            .arg(static_cast<int>(openedCollectionScope.kind()))
            .arg(scopeFileIdentity, scopeRootIdentity, imageIdentity);
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
