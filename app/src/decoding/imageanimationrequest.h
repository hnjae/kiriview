// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEANIMATIONREQUEST_H
#define KIRIVIEW_IMAGEANIMATIONREQUEST_H

#include "imagedecodeworkspace.h"
#include "imagesourcedata.h"

#include <QByteArray>
#include <memory>
#include <variant>

namespace kiriview {
struct ReaderAnimationPlaybackRequest
{
    QByteArray data;
    QByteArray format;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

struct ApngAnimationPlaybackRequest
{
    QByteArray data;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

struct WebPAnimationPlaybackRequest
{
    QByteArray data;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

struct JxlAnimationPlaybackRequest
{
    QByteArray data;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

struct HeifSequenceAnimationPlaybackRequest
{
    QByteArray data;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
    qsizetype retainedInputWorkspaceByteCount = 0;
};

struct ImageAnimationPlaybackRequest
{
    using Payload = std::variant<std::monostate, ReaderAnimationPlaybackRequest,
        ApngAnimationPlaybackRequest, WebPAnimationPlaybackRequest, JxlAnimationPlaybackRequest,
        HeifSequenceAnimationPlaybackRequest>;

    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    Payload payload;

    ImageAnimationPlaybackRequest() = default;
    ImageAnimationPlaybackRequest(ImageSourceDataLease sourceDataLease,
        ImageDecodeWorkspaceHold inputWorkspaceHold, Payload payload);
    ImageAnimationPlaybackRequest(const ImageAnimationPlaybackRequest&) = default;
    ImageAnimationPlaybackRequest(ImageAnimationPlaybackRequest&&) noexcept = default;
    ~ImageAnimationPlaybackRequest() = default;
    ImageAnimationPlaybackRequest& operator=(const ImageAnimationPlaybackRequest& other);
    ImageAnimationPlaybackRequest& operator=(ImageAnimationPlaybackRequest&& other) noexcept;
    friend void swap(
        ImageAnimationPlaybackRequest& left, ImageAnimationPlaybackRequest& right) noexcept;

    [[nodiscard]] bool isValid() const;
};

ImageAnimationPlaybackRequest readerAnimationPlaybackRequest(QByteArray data, QByteArray format,
    ImageSourceDataLease sourceDataLease = {},
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    ImageDecodeWorkspaceHold inputWorkspaceHold = {});
ImageAnimationPlaybackRequest apngAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease = {},
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    ImageDecodeWorkspaceHold inputWorkspaceHold = {});
ImageAnimationPlaybackRequest webpAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease = {},
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    ImageDecodeWorkspaceHold inputWorkspaceHold = {});
ImageAnimationPlaybackRequest jxlAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease = {},
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    ImageDecodeWorkspaceHold inputWorkspaceHold = {});
ImageAnimationPlaybackRequest heifSequenceAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease = {},
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    ImageDecodeWorkspaceHold inputWorkspaceHold = {});
}

#endif
