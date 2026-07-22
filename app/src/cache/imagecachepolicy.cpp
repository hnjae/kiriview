// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagecachepolicy.h"

#include <algorithm>

namespace {
constexpr qsizetype displayImageCacheSystemMemoryDivisor = 16;
constexpr qsizetype displayImageCachePreferredBudget = qsizetype { 512 } * 1024 * 1024;
constexpr qsizetype predecodeCachePreferredBudget = qsizetype { 1024 } * 1024 * 1024;
constexpr qsizetype predecodeCacheSystemMemoryDivisor = 8;
constexpr qsizetype thumbnailCachePreferredBudget = qsizetype { 64 } * 1024 * 1024;
constexpr qsizetype thumbnailCacheSystemMemoryDivisor = 64;

qsizetype systemMemoryCappedByteBudget(
    qsizetype preferredByteBudget, qsizetype systemMemoryByteSize, qsizetype memoryDivisor)
{
    if (preferredByteBudget <= 0) {
        return 0;
    }
    if (systemMemoryByteSize <= 0 || memoryDivisor <= 0) {
        return preferredByteBudget;
    }
    return std::min(preferredByteBudget, systemMemoryByteSize / memoryDivisor);
}
}

namespace kiriview {
qsizetype displayImageCacheByteBudgetForSystemMemory(
    qsizetype systemMemoryByteSize, qsizetype preferredByteBudget)
{
    return systemMemoryCappedByteBudget(
        preferredByteBudget, systemMemoryByteSize, displayImageCacheSystemMemoryDivisor);
}

qsizetype displayImageCachePreferredByteBudget() { return displayImageCachePreferredBudget; }

qsizetype predecodeCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize)
{
    return systemMemoryCappedByteBudget(
        predecodeCachePreferredBudget, systemMemoryByteSize, predecodeCacheSystemMemoryDivisor);
}

qsizetype thumbnailCachePreferredByteBudget() { return thumbnailCachePreferredBudget; }

qsizetype thumbnailCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize)
{
    return systemMemoryCappedByteBudget(
        thumbnailCachePreferredBudget, systemMemoryByteSize, thumbnailCacheSystemMemoryDivisor);
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
