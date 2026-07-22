// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagerendering.h"

#include <QRectF>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace kiriview {
QRectF imageTargetRect(QSize imageSize, QSizeF boundsSize)
{
    if (imageSize.isEmpty() || boundsSize.isEmpty()) {
        return {};
    }
    const qreal scale = std::min(
        boundsSize.width() / imageSize.width(), boundsSize.height() / imageSize.height());
    const QSizeF targetSize = QSizeF(imageSize) * scale;
    return QRectF(QPointF((boundsSize.width() - targetSize.width()) / 2.0,
                      (boundsSize.height() - targetSize.height()) / 2.0),
        targetSize);
}

QSize scaledImageSizeToFit(QSizeF imageSize, QSize boundsSize)
{
    if (imageSize.isEmpty() || boundsSize.isEmpty() || !std::isfinite(imageSize.width())
        || !std::isfinite(imageSize.height())) {
        return {};
    }
    const qreal scale = std::min({ qreal(1), boundsSize.width() / imageSize.width(),
        boundsSize.height() / imageSize.height() });
    if (!std::isfinite(scale) || scale <= 0.0) {
        return {};
    }
    return QSize(std::clamp(qCeil(imageSize.width() * scale), 1, boundsSize.width()),
        std::clamp(qCeil(imageSize.height() * scale), 1, boundsSize.height()));
}

QSize firstDisplayScaledImageSize(QSize imageSize, QSize logicalViewportSize)
{
    const QSize scaled = scaledImageSizeToFit(QSizeF(imageSize), logicalViewportSize);
    return scaled.isEmpty() || scaled == imageSize ? QSize() : scaled;
}

QImage displayReadyImage(const QImage& image)
{
    if (image.format() == QImage::Format_RGBA8888_Premultiplied) {
        return image;
    }
    return image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
}

}
