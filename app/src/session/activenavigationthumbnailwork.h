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
#include <QImage>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <vector>

namespace kiriview {
enum class ActiveNavigationThumbnailWorkKind {
    Foreground,
    Background,
};

struct ActiveNavigationThumbnailSchedulingSnapshot
{
    quint64 navigationGeneration = 0;
    std::vector<ThumbnailSourceRevisionKey> rows;
};

struct ActiveNavigationThumbnailWorkId
{
    quint64 value = 0;

    [[nodiscard]] bool isValid() const { return value != 0; }
    bool operator==(const ActiveNavigationThumbnailWorkId& other) const
    {
        return value == other.value;
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
    ThumbnailSourceRevisionKey sourceKey;
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

struct ActiveNavigationThumbnailWorkRequest
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceRevisionKey sourceKey;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ThumbnailSourceAdapterPlan sourcePlan;
};

enum class ActiveNavigationThumbnailWorkResultKind {
    Ready,
    Failed,
};

struct ActiveNavigationThumbnailWorkResult
{
    ActiveNavigationThumbnailWorkResultKind kind = ActiveNavigationThumbnailWorkResultKind::Failed;
    QImage image;
    ActiveNavigationThumbnailFailureKind failureKind
        = ActiveNavigationThumbnailFailureKind::GenerationFailed;
    QString errorString;
};

struct ActiveNavigationThumbnailWorkCompletion
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceRevisionKey sourceKey;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ActiveNavigationThumbnailWorkResult result;
};

using ThumbnailSourceAdapter
    = std::function<ThumbnailSourceAdapterPlan(ThumbnailSourceAdapterRequest)>;

ThumbnailSourceAdapter defaultThumbnailSourceAdapter();
}

#endif
