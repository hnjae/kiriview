// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOTHUMBNAILEXTRACTIONADAPTER_H
#define KIRIVIEW_VIDEOTHUMBNAILEXTRACTIONADAPTER_H

#include "async/imageiojob.h"

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#include <functional>

class QObject;

namespace kiriview {
using ThumbnailVideoExtractionProvider = std::function<ImageIoJob(
    QObject*, VideoThumbnailExtractionRequest, VideoThumbnailExtractionCallback)>;

ImageIoJob startThumbnailVideoExtractionJob(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback callback);
}

#endif
