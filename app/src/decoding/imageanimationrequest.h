// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEANIMATIONREQUEST_H
#define KIRIVIEW_IMAGEANIMATIONREQUEST_H

#include "imagesourcedata.h"

#include <QByteArray>
#include <variant>

namespace kiriview {
struct ReaderAnimationPlaybackRequest
{
    QByteArray data;
    QByteArray format;
};

struct ApngAnimationPlaybackRequest
{
    QByteArray data;
};

struct WebPAnimationPlaybackRequest
{
    QByteArray data;
};

struct JxlAnimationPlaybackRequest
{
    QByteArray data;
};

struct HeifSequenceAnimationPlaybackRequest
{
    QByteArray data;
};

struct ImageAnimationPlaybackRequest
{
    using Payload = std::variant<std::monostate, ReaderAnimationPlaybackRequest,
        ApngAnimationPlaybackRequest, WebPAnimationPlaybackRequest, JxlAnimationPlaybackRequest,
        HeifSequenceAnimationPlaybackRequest>;

    Payload payload;
    ImageSourceDataLease sourceDataLease;

    [[nodiscard]] bool isValid() const;
};

ImageAnimationPlaybackRequest readerAnimationPlaybackRequest(
    QByteArray data, QByteArray format, ImageSourceDataLease sourceDataLease = {});
ImageAnimationPlaybackRequest apngAnimationPlaybackRequest(
    QByteArray data, ImageSourceDataLease sourceDataLease = {});
ImageAnimationPlaybackRequest webpAnimationPlaybackRequest(
    QByteArray data, ImageSourceDataLease sourceDataLease = {});
ImageAnimationPlaybackRequest jxlAnimationPlaybackRequest(
    QByteArray data, ImageSourceDataLease sourceDataLease = {});
ImageAnimationPlaybackRequest heifSequenceAnimationPlaybackRequest(
    QByteArray data, ImageSourceDataLease sourceDataLease = {});
}

#endif
