// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_QT_MULTIMEDIA_THUMBNAIL_BACKEND_P_H
#define KIRIVIEW_QT_MULTIMEDIA_THUMBNAIL_BACKEND_P_H

#include "videothumbnailextraction_p.h"

#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QVideoSink>

#include <memory>

namespace kiriview::detail {

struct QtVideoThumbnailBackendResources
{
    std::unique_ptr<QMediaPlayer> player;
    std::unique_ptr<QVideoSink> sink;
};

[[nodiscard]] auto projectQtVideoThumbnailMediaStatus(QMediaPlayer::MediaStatus status)
    -> VideoThumbnailBackendMediaStatus;
[[nodiscard]] auto projectQtVideoThumbnailError(QMediaPlayer::Error error)
    -> VideoThumbnailBackendError;
[[nodiscard]] auto projectQtVideoThumbnailMetadata(const QMediaMetaData& metadata)
    -> VideoThumbnailEmbeddedImages;

[[nodiscard]] auto createQtVideoThumbnailBackend(QtVideoThumbnailBackendResources resources)
    -> std::unique_ptr<VideoThumbnailBackend>;

} // namespace kiriview::detail

#endif
