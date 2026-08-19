// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_OPENEDCOLLECTIONTHUMBNAILPOLICY_H
#define KIRIVIEW_OPENEDCOLLECTIONTHUMBNAILPOLICY_H

#include "archive/mediaentrysourcebackend.h"
#include "location/imagelocation.h"

#include <QString>
#include <QUrl>

namespace kiriview {
enum class OpenedCollectionThumbnailSourcePlanKind {
    Unsupported,
    CacheableOpenedCollectionEntry,
};

struct OpenedCollectionThumbnailSourcePlan
{
    OpenedCollectionThumbnailSourcePlanKind kind
        = OpenedCollectionThumbnailSourcePlanKind::Unsupported;
    OpenedCollectionScopeLocation openedCollectionScope;
};

bool openedCollectionEntrySupportsThumbnailContentIdentity(
    const OpenedCollectionScopeLocation& openedCollectionScope,
    MediaEntrySourceEntryKind entryKind);
bool openedCollectionEntryPathSupportsThumbnailContentIdentity(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath);
OpenedCollectionThumbnailSourcePlan openedCollectionThumbnailSourcePlan(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& entryUrl,
    MediaEntrySourceEntryKind entryKind);
}

#endif
