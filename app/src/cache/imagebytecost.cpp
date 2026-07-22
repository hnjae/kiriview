// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagebytecost.h"

#include "cache/imagebyteaccounting.h"

#include <limits>

namespace kiriview {
qsizetype imageByteCost(const QImage& image)
{
    if (image.isNull()) {
        return 0;
    }
    return image.sizeInBytes();
}

qsizetype estimatedRgbaByteCost(QSize size)
{
    if (size.isEmpty()) {
        return 0;
    }
    constexpr qsizetype bytesPerPixel = 4;
    const qsizetype width = size.width();
    const qsizetype height = size.height();
    if (width > std::numeric_limits<qsizetype>::max() / height / bytesPerPixel) {
        return std::numeric_limits<qsizetype>::max();
    }
    return width * height * bytesPerPixel;
}
}
