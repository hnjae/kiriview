// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "openedcollectionthumbnailpolicy.h"

#include "decoding/imageformatregistry.h"
#include "location/imagedocumentlocation.h"

namespace {
bool rootSchemeSupportsThumbnailContentIdentity(const QString& rootScheme)
{
    return rootScheme == QStringLiteral("zip");
}
}

namespace kiriview {
bool openedCollectionEntrySupportsThumbnailContentIdentity(
    const OpenedCollectionScopeLocation& openedCollectionScope, MediaEntrySourceEntryKind entryKind)
{
    return entryKind == MediaEntrySourceEntryKind::Image
        && rootSchemeSupportsThumbnailContentIdentity(openedCollectionScope.rootUrl().scheme());
}

bool openedCollectionEntryPathSupportsThumbnailContentIdentity(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath)
{
    return isSupportedImageFileName(entryPath)
        && openedCollectionEntrySupportsThumbnailContentIdentity(
            openedCollectionScope, MediaEntrySourceEntryKind::Image);
}

OpenedCollectionThumbnailSourcePlan openedCollectionThumbnailSourcePlan(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& entryUrl,
    MediaEntrySourceEntryKind entryKind)
{
    if (!openedCollectionEntrySupportsThumbnailContentIdentity(openedCollectionScope, entryKind)
        || !openedCollectionScopeContainsUrl(openedCollectionScope, entryUrl)) {
        return {};
    }

    return OpenedCollectionThumbnailSourcePlan {
        OpenedCollectionThumbnailSourcePlanKind::CacheableOpenedCollectionEntry,
        openedCollectionScope,
    };
}
}
