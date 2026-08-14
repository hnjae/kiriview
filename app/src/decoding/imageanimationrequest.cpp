// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageanimationrequest.h"

#include <utility>

namespace kiriview {
ImageAnimationPlaybackRequest::ImageAnimationPlaybackRequest(ImageSourceDataLease sourceDataLease,
    ImageDecodeWorkspaceHold inputWorkspaceHold, Payload payload)
    : sourceDataLease(std::move(sourceDataLease))
    , inputWorkspaceHold(std::move(inputWorkspaceHold))
    , payload(std::move(payload))
{
}

ImageAnimationPlaybackRequest& ImageAnimationPlaybackRequest::operator=(
    const ImageAnimationPlaybackRequest& other)
{
    if (this != &other) {
        ImageAnimationPlaybackRequest replacement(other);
        swap(*this, replacement);
    }
    return *this;
}

ImageAnimationPlaybackRequest& ImageAnimationPlaybackRequest::operator=(
    ImageAnimationPlaybackRequest&& other) noexcept
{
    if (this != &other) {
        ImageAnimationPlaybackRequest replacement(std::move(other));
        swap(*this, replacement);
    }
    return *this;
}

void swap(ImageAnimationPlaybackRequest& left, ImageAnimationPlaybackRequest& right) noexcept
{
    using std::swap;
    swap(left.sourceDataLease, right.sourceDataLease);
    swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
    swap(left.payload, right.payload);
}

bool ImageAnimationPlaybackRequest::isValid() const
{
    return !std::holds_alternative<std::monostate>(payload);
}

ImageAnimationPlaybackRequest readerAnimationPlaybackRequest(QByteArray data, QByteArray format,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    return ImageAnimationPlaybackRequest {
        std::move(sourceDataLease),
        std::move(inputWorkspaceHold),
        ReaderAnimationPlaybackRequest {
            std::move(data),
            std::move(format),
            std::move(workspaceBudget),
        },
    };
}

ImageAnimationPlaybackRequest apngAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    return ImageAnimationPlaybackRequest {
        std::move(sourceDataLease),
        std::move(inputWorkspaceHold),
        ApngAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
    };
}

ImageAnimationPlaybackRequest webpAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    return ImageAnimationPlaybackRequest {
        std::move(sourceDataLease),
        std::move(inputWorkspaceHold),
        WebPAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
    };
}

ImageAnimationPlaybackRequest jxlAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    return ImageAnimationPlaybackRequest {
        std::move(sourceDataLease),
        std::move(inputWorkspaceHold),
        JxlAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
    };
}

ImageAnimationPlaybackRequest heifSequenceAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    const qsizetype retainedInputWorkspaceByteCount = inputWorkspaceHold.reservedByteCount();
    return ImageAnimationPlaybackRequest {
        std::move(sourceDataLease),
        std::move(inputWorkspaceHold),
        HeifSequenceAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
            retainedInputWorkspaceByteCount,
        },
    };
}
}
