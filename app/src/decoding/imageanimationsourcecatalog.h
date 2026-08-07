// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEANIMATIONSOURCECATALOG_H
#define KIRIVIEW_IMAGEANIMATIONSOURCECATALOG_H

#include "animationframe.h"
#include "imageanimationrequest.h"

#include <QSize>
#include <QString>
#include <QVector>
#include <expected>

namespace kiriview {
class HeifSequenceReader;

struct ImageAnimationSourceCatalog
{
    QSize logicalSize;
    QVector<int> frameDurations;
    int repeatCount = 0;

    [[nodiscard]] bool isValid() const;
};

enum class ImageAnimationSourceCatalogFailureCause {
    InvalidSource,
    ResourceLimitExceeded,
};

struct ImageAnimationSourceCatalogFailure
{
    QString errorString;
    ImageAnimationSourceCatalogFailureCause cause
        = ImageAnimationSourceCatalogFailureCause::InvalidSource;
};

using ImageAnimationSourceCatalogResult
    = std::expected<ImageAnimationSourceCatalog, ImageAnimationSourceCatalogFailure>;

ImageAnimationSourceCatalogResult readImageAnimationSourceCatalog(
    const ImageAnimationPlaybackRequest& request);
ImageAnimationSourceCatalogResult readHeifSequenceAnimationSourceCatalog(
    HeifSequenceReader& reader, const AnimationFrame& firstFrame, int repeatCount);
}

#endif
