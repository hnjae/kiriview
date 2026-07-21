// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagecachepolicy.h"

#include "bridge/rustqtconversion.h"
#include "kiriview/src/policy/cachebudget.cxx.h"

namespace kiriview {
qsizetype displayImageCacheByteBudgetForSystemMemory(
    qsizetype systemMemoryByteSize, qsizetype preferredByteBudget)
{
    return Bridge::qtByteSize(rustDisplayImageCacheByteBudgetForSystemMemory(
        Bridge::rustByteSize(systemMemoryByteSize), Bridge::rustByteSize(preferredByteBudget)));
}

qsizetype displayImageCachePreferredByteBudget()
{
    return Bridge::qtByteSize(rustDisplayImageCachePreferredByteBudget());
}

qsizetype predecodeCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize)
{
    return Bridge::qtByteSize(
        rustPredecodeCacheByteBudgetForSystemMemory(Bridge::rustByteSize(systemMemoryByteSize)));
}

qsizetype thumbnailCachePreferredByteBudget()
{
    return Bridge::qtByteSize(rustThumbnailCachePreferredByteBudget());
}

qsizetype thumbnailCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize)
{
    return Bridge::qtByteSize(
        rustThumbnailCacheByteBudgetForSystemMemory(Bridge::rustByteSize(systemMemoryByteSize)));
}

ImageCacheBudgets resolvedImageCacheBudgets(
    ImageCacheBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    const qsizetype displayImagePreferredByteBudget
        = request.displayImageCachePreferredByteBudget > 0
        ? request.displayImageCachePreferredByteBudget
        : displayImageCachePreferredByteBudget();
    const qsizetype predecodeCacheByteBudget = request.predecodeCacheByteBudget > 0
        ? request.predecodeCacheByteBudget
        : predecodeCacheByteBudgetForSystemMemory(systemMemory.physicalByteSize);
    const qsizetype displayImageCacheByteBudget = request.displayImageCacheByteBudget > 0
        ? request.displayImageCacheByteBudget
        : displayImageCacheByteBudgetForSystemMemory(
              systemMemory.physicalByteSize, displayImagePreferredByteBudget);
    const qsizetype thumbnailCacheByteBudget = request.thumbnailCacheByteBudget > 0
        ? request.thumbnailCacheByteBudget
        : thumbnailCacheByteBudgetForSystemMemory(systemMemory.physicalByteSize);
    return ImageCacheBudgets {
        predecodeCacheByteBudget,
        displayImageCacheByteBudget,
        thumbnailCacheByteBudget,
    };
}
}
