// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodedependencies.h"

#include "imagedataloading.h"
#include "kiriimagedecoder.h"

#include <utility>

namespace {
kiriview::ImageDataLoader imageDataLoader(kiriview::ImageWorkerScheduler workerScheduler,
    std::shared_ptr<kiriview::ImageSourceDataBudget> sourceDataBudget)
{
    return [workerScheduler = std::move(workerScheduler),
               sourceDataBudget = std::move(sourceDataBudget)](QObject* receiver,
               kiriview::ImageDecodeRequest request, kiriview::ImageDataCallback callback,
               kiriview::ErrorCallback errorCallback) {
        return kiriview::startStoredImageDataLoad(receiver, std::move(request), workerScheduler,
            sourceDataBudget, std::move(callback), std::move(errorCallback));
    };
}

kiriview::DecodedImageResult decodeImageDataWithDefaults(
    const QByteArray& data, const kiriview::ImageDecodeRequest& request)
{
    return kiriview::decodeImageData(data, request);
}
}

namespace kiriview {
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) -- std::function owns the
// heap-allocated callable captured by the returned dependency bundle.
ImageDecodeDependencies defaultImageDecodeDependencies()
{
    ImageWorkerScheduler workerScheduler = defaultImageWorkerScheduler();
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget = defaultImageSourceDataBudget();
    return ImageDecodeDependencies {
        imageDataLoader(workerScheduler, sourceDataBudget),
        decodeImageDataWithDefaults,
        defaultThumbnailCacheLookupProvider(workerScheduler),
        rawEmbeddedThumbnailPreviewResult,
        std::move(workerScheduler),
        std::move(sourceDataBudget),
    };
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

ImageDecodeDependencies imageDecodeDependenciesWithDefaults(ImageDecodeDependencies dependencies)
{
    ImageDecodeDependencies defaults = defaultImageDecodeDependencies();
    if (!dependencies.workerScheduler.isValid()) {
        dependencies.workerScheduler = std::move(defaults.workerScheduler);
    }
    if (dependencies.sourceDataBudget == nullptr) {
        dependencies.sourceDataBudget = std::move(defaults.sourceDataBudget);
    }
    if (!dependencies.dataLoader) {
        dependencies.dataLoader
            = imageDataLoader(dependencies.workerScheduler, dependencies.sourceDataBudget);
    }
    if (!dependencies.dataDecoder) {
        dependencies.dataDecoder = std::move(defaults.dataDecoder);
    }
    if (!dependencies.thumbnailPreviewLookupProvider) {
        dependencies.thumbnailPreviewLookupProvider
            = defaultThumbnailCacheLookupProvider(dependencies.workerScheduler);
    }
    if (!dependencies.rawEmbeddedThumbnailPreviewExtractor) {
        dependencies.rawEmbeddedThumbnailPreviewExtractor
            = std::move(defaults.rawEmbeddedThumbnailPreviewExtractor);
    }

    return dependencies;
}
}
