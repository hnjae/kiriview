// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOTHUMBNAILEXTRACTOR_H
#define KIRIVIEW_VIDEOTHUMBNAILEXTRACTOR_H

#include "async/imageiojob.h"
#include "async/timerscheduler.h"
#include "thumbnail/videothumbnailbackend.h"
#include "thumbnail/videothumbnailextractionworkflow.h"

#include <functional>

class QObject;

namespace kiriview {
using VideoThumbnailExtractionCallback = std::function<void(VideoThumbnailExtractionResult)>;
using VideoThumbnailExtractionProvider = std::function<ImageIoJob(
    QObject*, VideoThumbnailExtractionRequest, VideoThumbnailExtractionCallback)>;

struct VideoThumbnailExtractionDependencies
{
    VideoThumbnailBackendFactory backendFactory;
    TimerScheduler timerScheduler;
};

ImageIoJob startVideoThumbnailExtraction(QObject* receiver, VideoThumbnailExtractionRequest request,
    VideoThumbnailExtractionCallback callback,
    VideoThumbnailExtractionDependencies dependencies = {});
VideoThumbnailExtractionProvider videoThumbnailExtractionProvider(
    VideoThumbnailExtractionDependencies dependencies = {});
}

#endif
