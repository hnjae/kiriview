// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEDEPENDENCIES_H
#define KIRIVIEW_IMAGEDECODEDEPENDENCIES_H

#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "async/imagerefinementscheduler.h"
#include "async/imageworkerscheduler.h"
#include "decodedimageresult.h"
#include "imagedataloaderror.h"
#include "imagedecodepipeline.h"
#include "imagedecoderequest.h"
#include "imagedecodeworkspace.h"
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

struct ImageDecodeDependencies
{
    ImageDataLoader dataLoader;
    ImageDataDecodePlanner dataPlanner;
    ThumbnailCacheLookupProvider thumbnailPreviewLookupProvider;
    RawEmbeddedThumbnailPreviewExtractor rawEmbeddedThumbnailPreviewExtractor;
    ImageWorkerScheduler workerScheduler;
    ImageWorkerScheduler refinementScheduler;
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

ImageDecodeDependencies defaultImageDecodeDependencies();
ImageDecodeDependencies imageDecodeDependenciesWithDefaults(ImageDecodeDependencies dependencies);
}

#endif
