// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagelocation.h"

#include "location/imageurl.h"

namespace kiriview {
bool sameOpenedCollectionScopeLocation(
    const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
{
    return sameNormalizedUrl(left.fileUrl(), right.fileUrl())
        && sameNormalizedUrl(left.rootUrl(), right.rootUrl()) && left.kind() == right.kind();
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

    return QStringLiteral("displayed\x1f%1")
        .arg(normalizedUrlIdentityKey(location.imageUrl(), QUrl::FullyEncoded));
}
}
