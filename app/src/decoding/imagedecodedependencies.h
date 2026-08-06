// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEDEPENDENCIES_H
#define KIRIVIEW_IMAGEDECODEDEPENDENCIES_H

#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "decodedimageresult.h"
#include "imagedataloaderror.h"
#include "imagedecoderequest.h"
#include "imagesourcedata.h"
#include "rawthumbnailpreview.h"
#include "thumbnail/thumbnailcachelookup.h"

#include <QByteArray>
#include <functional>
#include <memory>

class QObject;

namespace kiriview {
using ImageDataLoader = std::function<ImageIoJob(
    QObject*, ImageDecodeRequest, ImageDataCallback, ImageDataLoadErrorCallback)>;
using ImageDataDecoder
    = std::function<DecodedImageResult(const QByteArray&, const ImageDecodeRequest&)>;

struct ImageDecodeDependencies
{
    ImageDataLoader dataLoader;
    ImageDataDecoder dataDecoder;
    ThumbnailCacheLookupProvider thumbnailPreviewLookupProvider;
    RawEmbeddedThumbnailPreviewExtractor rawEmbeddedThumbnailPreviewExtractor;
    ImageWorkerScheduler workerScheduler;
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget;
};

ImageDecodeDependencies defaultImageDecodeDependencies();
ImageDecodeDependencies imageDecodeDependenciesWithDefaults(ImageDecodeDependencies dependencies);
}

#endif
