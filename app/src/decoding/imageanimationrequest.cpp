// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageanimationrequest.h"

#include <utility>

namespace kiriview {
bool ImageAnimationPlaybackRequest::isValid() const
{
    return !std::holds_alternative<std::monostate>(payload);
}

ImageAnimationPlaybackRequest readerAnimationPlaybackRequest(QByteArray data, QByteArray format)
{
    return ImageAnimationPlaybackRequest {
        ReaderAnimationPlaybackRequest {
            std::move(data),
            std::move(format),
        },
    };
}

ImageAnimationPlaybackRequest apngAnimationPlaybackRequest(QByteArray data)
{
    return ImageAnimationPlaybackRequest {
        ApngAnimationPlaybackRequest {
            std::move(data),
        },
    };
}

ImageAnimationPlaybackRequest webpAnimationPlaybackRequest(QByteArray data)
{
    return ImageAnimationPlaybackRequest {
        WebPAnimationPlaybackRequest {
            std::move(data),
        },
    };
}

ImageAnimationPlaybackRequest jxlAnimationPlaybackRequest(QByteArray data)
{
    return ImageAnimationPlaybackRequest {
        JxlAnimationPlaybackRequest {
            std::move(data),
        },
    };
}

ImageAnimationPlaybackRequest heifSequenceAnimationPlaybackRequest(QByteArray data)
{
    return ImageAnimationPlaybackRequest {
        HeifSequenceAnimationPlaybackRequest {
            std::move(data),
        },
    };
}
}
