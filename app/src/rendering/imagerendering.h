// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGERENDERING_H
#define KIRIVIEW_IMAGERENDERING_H

#include "staticimage.h"

#include <QByteArray>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

namespace kiriview {
QRectF imageTargetRect(QSize imageSize, QSizeF boundsSize);
QSize scaledImageSizeToFit(QSizeF imageSize, QSize boundsSize);
QSize firstDisplayScaledImageSize(QSize imageSize, QSize logicalViewportSize);
QImage displayReadyImage(const QImage& image);
QImage copiedImageFromBytes(
    const QByteArray& bytes, QSize size, qsizetype bytesPerLine, QImage::Format format);
}

#endif
