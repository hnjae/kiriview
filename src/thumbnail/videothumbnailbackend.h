// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOTHUMBNAILBACKEND_H
#define KIRIVIEW_VIDEOTHUMBNAILBACKEND_H

#include <QImage>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>

class QObject;

namespace kiriview {
enum class VideoThumbnailBackendMediaStatus {
    Pending,
    Ready,
    EndOfMedia,
    Invalid,
};

struct VideoThumbnailBackendMediaFacts
{
    VideoThumbnailBackendMediaStatus status = VideoThumbnailBackendMediaStatus::Pending;
    qint64 durationMsec = 0;
    bool seekable = false;
};

struct VideoThumbnailEmbeddedImages
{
    QImage coverArt;
    QImage thumbnail;
};

struct VideoThumbnailBackendCallbacks
{
    std::function<void(VideoThumbnailBackendMediaFacts)> mediaFactsChanged;
    std::function<void(qint64)> positionChanged;
    std::function<void(QImage)> frameAvailable;
    std::function<void(VideoThumbnailEmbeddedImages)> metadataAvailable;
    std::function<void(QString)> errorOccurred;
};

class VideoThumbnailBackend
{
public:
    VideoThumbnailBackend() = default;
    virtual ~VideoThumbnailBackend() = default;

    virtual void setCallbacks(VideoThumbnailBackendCallbacks callbacks) = 0;
    virtual void setSource(const QUrl& sourceUrl) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void setPosition(qint64 position) = 0;
    Q_DISABLE_COPY(VideoThumbnailBackend)
};

using VideoThumbnailBackendFactory
    = std::function<std::unique_ptr<VideoThumbnailBackend>(QObject*)>;

std::unique_ptr<VideoThumbnailBackend> createDefaultVideoThumbnailBackend(QObject* parent);
}

#endif
