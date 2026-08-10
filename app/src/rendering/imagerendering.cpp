// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagerendering.h"

#include <QColorSpace>
#include <QList>
#include <QPoint>
#include <QRectF>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <utility>

namespace {
struct WorkspaceAdmittedImageBacking
{
    kiriview::ImageDecodeWorkspaceHold workspaceHold;
    QImage image;
};

void deleteWorkspaceAdmittedImageBacking(void* data)
{
    delete static_cast<WorkspaceAdmittedImageBacking*>(data);
}
}

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

QImage imageRetainingDecodeWorkspace(QImage image, ImageDecodeWorkspaceHold workspaceHold)
{
    if (image.isNull() || !workspaceHold.isManaged()) {
        return {};
    }

    const QColorSpace colorSpace = image.colorSpace();
    const QList<QRgb> colorTable = image.colorTable();
    const qreal devicePixelRatio = image.devicePixelRatio();
    const qint64 dotsPerMeterX = image.dotsPerMeterX();
    const qint64 dotsPerMeterY = image.dotsPerMeterY();
    const QPoint offset = image.offset();
    std::unique_ptr<WorkspaceAdmittedImageBacking> backing(new (std::nothrow)
            WorkspaceAdmittedImageBacking { std::move(workspaceHold), std::move(image) });
    if (backing == nullptr) {
        return {};
    }
    const uchar* const pixels = backing->image.constBits();
    // The backing image owns the physical pixels. Its workspace hold is destroyed only after the
    // last wrapper alias retires and the backing image has released those pixels.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    QImage admitted(const_cast<uchar*>(pixels), backing->image.width(), backing->image.height(),
        backing->image.bytesPerLine(), backing->image.format(), deleteWorkspaceAdmittedImageBacking,
        backing.get());
    if (admitted.isNull()) {
        return {};
    }
    [[maybe_unused]] WorkspaceAdmittedImageBacking* const admittedBacking = backing.release();
    Q_ASSERT(admittedBacking != nullptr);

    admitted.setColorSpace(colorSpace);
    if (!colorTable.isEmpty()) {
        admitted.setColorTable(colorTable);
    }
    admitted.setDevicePixelRatio(devicePixelRatio);
    admitted.setDotsPerMeterX(dotsPerMeterX);
    admitted.setDotsPerMeterY(dotsPerMeterY);
    admitted.setOffset(offset);
    if (admitted.constBits() != pixels) {
        return {};
    }
    return admitted;
}

QImage copiedImageFromBytes(
    const QByteArray& bytes, QSize size, qsizetype bytesPerLine, QImage::Format format)
{
    if (bytes.isEmpty() || size.isEmpty() || bytesPerLine <= 0
        || bytesPerLine > bytes.size() / size.height()) {
        return {};
    }

    const QImage borrowedImage(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- QImage byte API.
        reinterpret_cast<const uchar*>(bytes.constData()), size.width(), size.height(),
        bytesPerLine, format);
    return borrowedImage.copy();
}

}
