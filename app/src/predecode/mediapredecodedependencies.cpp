// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediapredecodedependencies.h"

#include <utility>

namespace kiriview {
MediaPredecodeDependencies resolveMediaPredecodeDependencies(
    MediaPredecodeDependencyOverrides overrides)
{
    const SystemMemorySnapshot systemMemory
        = resolveSystemMemorySnapshot(overrides.systemMemorySnapshot);
    const qsizetype cacheByteBudget = overrides.cacheBudgetRequest.predecodeCacheByteBudget > 0
        ? overrides.cacheBudgetRequest.predecodeCacheByteBudget
        : predecodeCacheByteBudgetForSystemMemory(systemMemory.physicalByteSize);
    if (overrides.imageDecode.sourceDataBudget == nullptr) {
        overrides.imageDecode.sourceDataBudget
            = imageSourceDataBudgetForSystemMemory({}, systemMemory);
    }
    if (overrides.imageDecode.workspaceBudget == nullptr) {
        overrides.imageDecode.workspaceBudget
            = imageDecodeWorkspaceBudgetForSystemMemory({}, systemMemory);
    }

    return MediaPredecodeDependencies {
        imageDecodeDependenciesWithDefaults(std::move(overrides.imageDecode)),
        std::move(overrides.powerSaver),
        cacheByteBudget,
        timerSchedulerWithDefaults(std::move(overrides.timerScheduler)),
    };
}
}
