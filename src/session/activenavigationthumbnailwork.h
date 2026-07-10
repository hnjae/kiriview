// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORK_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORK_H

#include "location/imagelocation.h"
#include "location/sourcekey.h"
#include "session/activenavigationthumbnaildemand.h"
#include "session/activenavigationthumbnailprojection.h"
#include "thumbnail/thumbnailoriginalidentity.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <functional>

namespace kiriview {
enum class ActiveNavigationThumbnailWorkKind {
    Foreground,
    Background,
};

struct ActiveNavigationThumbnailWorkId
{
    quint64 value = 0;

    bool isValid() const { return value != 0; }
    bool operator==(const ActiveNavigationThumbnailWorkId& other) const
    {
        return value == other.value;
    }
    bool operator!=(const ActiveNavigationThumbnailWorkId& other) const
    {
        return !(*this == other);
    }
};

enum class ActiveNavigationThumbnailFailureKind {
    CacheLookupProviderUnavailable,
    CacheLookupInvalid,
    CacheLookupFailed,
    GenerationFailed,
    ImageStoreInsertFailed,
    GenerationProviderUnavailable,
};

enum class ThumbnailSourceAdapterPlanKind {
    Unsupported,
    CacheableLocalFile,
    CacheableOpenedCollectionEntry,
    InMemoryOnly,
};

struct ThumbnailSourceAdapterRequest
{
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailDemandBucket requestedBucket
        = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailDemandPriority priority
        = ActiveNavigationThumbnailDemandPriority::Nearby;
};

struct ThumbnailSourceAdapterPlan
{
    ThumbnailSourceAdapterPlanKind kind = ThumbnailSourceAdapterPlanKind::Unsupported;
    QByteArray localPathBytes;
    ThumbnailOriginalIdentity originalIdentity;
    OpenedCollectionScopeLocation openedCollectionScope;
};

using ThumbnailSourceAdapter
    = std::function<ThumbnailSourceAdapterPlan(ThumbnailSourceAdapterRequest)>;

ThumbnailSourceAdapter defaultThumbnailSourceAdapter();
}

#endif
