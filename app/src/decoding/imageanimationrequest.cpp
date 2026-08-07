// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageanimationrequest.h"

#include <utility>

namespace kiriview {
bool ImageAnimationPlaybackRequest::isValid() const
{
    return !std::holds_alternative<std::monostate>(payload);
}

ImageAnimationPlaybackRequest readerAnimationPlaybackRequest(QByteArray data, QByteArray format,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    return ImageAnimationPlaybackRequest {
        ReaderAnimationPlaybackRequest {
            std::move(data),
            std::move(format),
            std::move(workspaceBudget),
        },
        std::move(sourceDataLease),
    };
}

ImageAnimationPlaybackRequest apngAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    return ImageAnimationPlaybackRequest {
        ApngAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
        std::move(sourceDataLease),
    };
}

ImageAnimationPlaybackRequest webpAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    return ImageAnimationPlaybackRequest {
        WebPAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
        std::move(sourceDataLease),
    };
}

ImageAnimationPlaybackRequest jxlAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    return ImageAnimationPlaybackRequest {
        JxlAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
        std::move(sourceDataLease),
    };
}

ImageAnimationPlaybackRequest heifSequenceAnimationPlaybackRequest(QByteArray data,
    ImageSourceDataLease sourceDataLease,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    return ImageAnimationPlaybackRequest {
        HeifSequenceAnimationPlaybackRequest {
            std::move(data),
            std::move(workspaceBudget),
        },
        std::move(sourceDataLease),
    };
}
}
