// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGECACHEPOLICY_H
#define KIRIVIEW_IMAGECACHEPOLICY_H

#include "system/systemmemory.h"

#include <QtGlobal>
namespace kiriview {
struct ImageCacheBudgetRequest
{
    qsizetype predecodeCacheByteBudget = 0;
    qsizetype displayImageCacheByteBudget = 0;
    qsizetype displayImageCachePreferredByteBudget = 0;
    qsizetype thumbnailCacheByteBudget = 0;
};

struct ImageCacheBudgets
{
    qsizetype predecodeCacheByteBudget = 0;
    qsizetype displayImageCacheByteBudget = 0;
    qsizetype thumbnailCacheByteBudget = 0;
};

qsizetype predecodeCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize);
qsizetype thumbnailCachePreferredByteBudget();
qsizetype thumbnailCacheByteBudgetForSystemMemory(qsizetype systemMemoryByteSize);
qsizetype displayImageCachePreferredByteBudget();
qsizetype displayImageCacheByteBudgetForSystemMemory(
    qsizetype systemMemoryByteSize, qsizetype preferredByteBudget);
ImageCacheBudgets resolvedImageCacheBudgets(
    ImageCacheBudgetRequest request, SystemMemorySnapshot systemMemory);
}

#endif
