// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimagedisplaysourcehelpers_p.h"

#include "imagerendering.h"

namespace kiriview {
QSize boundedPreviewSize(QSize imageSize, int maximumLongEdge)
{
    return scaledImageSizeToFit(QSizeF(imageSize), QSize(maximumLongEdge, maximumLongEdge));
}

QImage scaledDisplayImage(const QImage& image, QSize size)
{
    if (image.isNull() || size.isEmpty()) {
        return {};
    }
    if (image.size() == size) {
        return displayReadyImage(image);
    }
    return displayReadyImage(image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void setStaticImageDisplaySourceError(QString* errorString, const QString& message)
{
    if (errorString != nullptr) {
        *errorString = message;
    }
}
}
