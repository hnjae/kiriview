// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimedependencies.h"

#include "archive/mediaentrysourcestore.h"

#include <QThread>
#include <utility>

namespace {
bool shouldUseMediaEntrySourceStore(
    const kiriview::ImageDocumentRuntimeDependencyOverrides& overrides)
{
    return overrides.mediaEntrySourceFactory
        || (!overrides.candidateProvider.openedCollectionCandidates
            && !overrides.imageDecode.dataLoader);
}

}

namespace kiriview {
ImageDocumentRuntimeDependencies::~ImageDocumentRuntimeDependencies() = default;

ImageCacheBudgetRequest imageDocumentCacheBudgetRequestWithDefaults(ImageCacheBudgetRequest request)
{
    if (request.displayImageCachePreferredByteBudget <= 0) {
        request.displayImageCachePreferredByteBudget = displayImageCachePreferredByteBudget();
    }
    return request;
}

ImageCacheBudgets resolveImageDocumentCacheBudgets(ImageCacheBudgetRequest request)
{
    return resolveImageDocumentCacheBudgets(request, systemMemorySnapshot());
}

ImageCacheBudgets resolveImageDocumentCacheBudgets(
    ImageCacheBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    request = imageDocumentCacheBudgetRequestWithDefaults(request);
    return resolvedImageCacheBudgets(request, systemMemory);
}

ImageDocumentRuntimeDependencies resolveImageDocumentRuntimeDependencies(
    ImageDocumentRuntimeDependencyOverrides overrides)
{
    const bool useMediaEntrySourceStore = shouldUseMediaEntrySourceStore(overrides);
    const SystemMemorySnapshot systemMemory
        = resolveSystemMemorySnapshot(overrides.systemMemorySnapshot);
    MediaEntrySourceFactory mediaEntrySourceFactory = std::move(overrides.mediaEntrySourceFactory);
    overrides.mediaEntrySourceFactory = {};
    if (overrides.imageDecode.sourceDataBudget == nullptr) {
        overrides.imageDecode.sourceDataBudget
            = imageSourceDataBudgetForSystemMemory({}, systemMemory);
    }
    if (overrides.imageDecode.workspaceBudget == nullptr) {
        overrides.imageDecode.workspaceBudget
            = imageDecodeWorkspaceBudgetForSystemMemory({}, systemMemory);
    }
    overrides.imageDecode = imageDecodeDependenciesWithDefaults(std::move(overrides.imageDecode));
    overrides.candidateProvider = imageDocumentPageNavigationCandidateProviderWithDefaults(
        std::move(overrides.candidateProvider), overrides.imageDecode.workerScheduler,
        std::move(overrides.directoryItemListProvider));
    overrides.fileDeletionProvider
        = fileDeletionProviderWithDefault(std::move(overrides.fileDeletionProvider));
    overrides.predecodeTimerScheduler
        = timerSchedulerWithDefaults(std::move(overrides.predecodeTimerScheduler));
    if (!overrides.predecodeThreadCountProvider) {
        overrides.predecodeThreadCountProvider = []() { return QThread::idealThreadCount(); };
    }
    const ImageCacheBudgets cacheBudgets
        = resolveImageDocumentCacheBudgets(overrides.cacheBudgetRequest, systemMemory);

    std::unique_ptr<MediaEntrySourceStore> mediaEntrySourceStore;
    if (useMediaEntrySourceStore) {
        mediaEntrySourceStore
            = std::make_unique<MediaEntrySourceStore>(std::move(mediaEntrySourceFactory),
                overrides.imageDecode.workerScheduler, overrides.imageDecode.sourceDataBudget);
        overrides.candidateProvider
            = mediaEntrySourceStore->wrapCandidateProvider(std::move(overrides.candidateProvider));
        overrides.imageDecode
            = mediaEntrySourceStore->wrapDecodeDependencies(std::move(overrides.imageDecode));
    }

    return ImageDocumentRuntimeDependencies {
        std::move(overrides.candidateProvider),
        std::move(overrides.imageDecode),
        std::move(overrides.fileDeletionProvider),
        std::move(overrides.powerSaver),
        std::move(overrides.predecodeTimerScheduler),
        std::move(overrides.predecodeThreadCountProvider),
        cacheBudgets,
        std::move(mediaEntrySourceStore),
        std::move(overrides.externalPredecodedImageFinder),
        std::move(overrides.externalPredecodeDisplayOutputReclaimer),
        overrides.ordinaryDirectMediaPredecodeEnabled,
    };
}
}
