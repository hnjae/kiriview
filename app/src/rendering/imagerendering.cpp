// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagerendering.h"

#include "bridge/qtgeometryconversion.h"
#include "kiriview/src/policy/imagerendergeometry.cxx.h"

#include <QRectF>

namespace kiriview {
QRectF imageTargetRect(QSize imageSize, QSizeF boundsSize)
{
    return Bridge::qtRectF(rustImageTargetRect(Bridge::rustSize<RustImageRenderSize>(imageSize),
        Bridge::rustSizeF<RustImageRenderSizeF>(boundsSize)));
}

QSize scaledImageSizeToFit(QSizeF imageSize, QSize boundsSize)
{
    return Bridge::qtSize(
        rustScaledImageSizeToFit(Bridge::rustSizeF<RustImageRenderSizeF>(imageSize),
            Bridge::rustSize<RustImageRenderSize>(boundsSize)));
}

QSize firstDisplayScaledImageSize(QSize imageSize, QSize logicalViewportSize)
{
    return Bridge::qtSize(
        rustFirstDisplayScaledImageSize(Bridge::rustSize<RustImageRenderSize>(imageSize),
            Bridge::rustSize<RustImageRenderSize>(logicalViewportSize)));
}

QImage displayReadyImage(const QImage& image)
{
    if (image.format() == QImage::Format_RGBA8888_Premultiplied) {
        return image;
    }
    return image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
}

}
