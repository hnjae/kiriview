// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodedependencies.h"

#include "imagedataloading.h"

#include <utility>

namespace {
kiriview::ImageDataLoader imageDataLoader(kiriview::ImageWorkerScheduler workerScheduler,
    std::shared_ptr<kiriview::ImageSourceDataBudget> sourceDataBudget)
{
    return [workerScheduler = std::move(workerScheduler),
               sourceDataBudget = std::move(sourceDataBudget)](QObject* receiver,
               kiriview::ImageDecodeRequest request, kiriview::ImageDataCallback callback,
               kiriview::ImageDataLoadErrorCallback errorCallback) {
        return kiriview::startStoredImageDataLoad(receiver, std::move(request), workerScheduler,
            sourceDataBudget, std::move(callback), std::move(errorCallback));
    };
}

}

namespace kiriview {
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) -- std::function owns the
// heap-allocated callable captured by the returned dependency bundle.
ImageDecodeDependencies defaultImageDecodeDependencies()
{
    ImageWorkerScheduler workerScheduler = defaultImageWorkerScheduler();
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget = defaultImageSourceDataBudget();
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget
        = defaultImageDecodeWorkspaceBudget();
    ImageDecodeDependencies dependencies;
    dependencies.dataLoader = imageDataLoader(workerScheduler, sourceDataBudget);
    dependencies.dataPlanner = defaultImageDataDecodePlanner(workspaceBudget);
    dependencies.thumbnailPreviewLookupProvider
        = defaultThumbnailCacheLookupProvider(workerScheduler, workspaceBudget);
    dependencies.rawEmbeddedThumbnailPreviewExtractor = admittedRawEmbeddedThumbnailPreviewResult;
    dependencies.workerScheduler = workerScheduler;
    dependencies.refinementScheduler = defaultImageRefinementScheduler();
    dependencies.sourceDataBudget = std::move(sourceDataBudget);
    dependencies.workspaceBudget = std::move(workspaceBudget);
    return dependencies;
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

ImageDecodeDependencies imageDecodeDependenciesWithDefaults(ImageDecodeDependencies dependencies)
{
    if (!dependencies.refinementScheduler.isValid()) {
        dependencies.refinementScheduler = defaultImageRefinementScheduler();
    }
    if (!dependencies.workerScheduler.isValid()) {
        dependencies.workerScheduler = defaultImageWorkerScheduler();
    }
    if (dependencies.sourceDataBudget == nullptr) {
        dependencies.sourceDataBudget = defaultImageSourceDataBudget();
    }
    if (dependencies.workspaceBudget == nullptr) {
        dependencies.workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    if (!dependencies.dataLoader) {
        dependencies.dataLoader
            = imageDataLoader(dependencies.workerScheduler, dependencies.sourceDataBudget);
    }
    if (!dependencies.dataPlanner) {
        dependencies.dataPlanner = defaultImageDataDecodePlanner(dependencies.workspaceBudget);
    }
    if (!dependencies.thumbnailPreviewLookupProvider) {
        dependencies.thumbnailPreviewLookupProvider = defaultThumbnailCacheLookupProvider(
            dependencies.workerScheduler, dependencies.workspaceBudget);
    }
    if (!dependencies.rawEmbeddedThumbnailPreviewExtractor) {
        dependencies.rawEmbeddedThumbnailPreviewExtractor
            = admittedRawEmbeddedThumbnailPreviewResult;
    }

    return dependencies;
}
}
