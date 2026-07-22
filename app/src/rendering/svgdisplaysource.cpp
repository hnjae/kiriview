// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "svgdisplaysource.h"

#include "bridge/rustqtconversion.h"
#include "imagerendering.h"
#include "kiriview/src/support/svgrenderer.cxx.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <QByteArray>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QtMath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace {
QImage imageFromPremultipliedRgbaBytes(const QByteArray& bytes, QSize size)
{
    if (bytes.isEmpty() || size.isEmpty()) {
        return {};
    }

    const std::int64_t expectedSize
        = static_cast<std::int64_t>(size.width()) * static_cast<std::int64_t>(size.height()) * 4;
    if (expectedSize <= 0 || expectedSize > std::numeric_limits<qsizetype>::max()
        || bytes.size() != static_cast<qsizetype>(expectedSize)) {
        return {};
    }

    const QImage image(reinterpret_cast<const uchar*>(bytes.constData()), size.width(),
        size.height(), QImage::Format_RGBA8888_Premultiplied);
    return image.copy();
}

QByteArray renderSvgImageBytes(const QByteArray& data, QSize size)
{
    if (size.isEmpty()) {
        return {};
    }

    return kiriview::Bridge::qtByteArray(kiriview::rustRenderSvgImage(
        kiriview::Bridge::rustBytes(data), size.width(), size.height()));
}

QImage renderSvgImage(const QByteArray& data, QSize size)
{
    return imageFromPremultipliedRgbaBytes(renderSvgImageBytes(data, size), size);
}

QSize svgFirstDisplayPreviewSize(QSize imageSize, QSize logicalViewportSize)
{
    if (imageSize.isEmpty() || logicalViewportSize.isEmpty()) {
        return {};
    }

    const QRectF targetRect = kiriview::imageTargetRect(imageSize, QSizeF(logicalViewportSize));
    if (targetRect.isEmpty()) {
        return {};
    }

    return QSize {
        std::clamp(qCeil(targetRect.width()), 1, logicalViewportSize.width()),
        std::clamp(qCeil(targetRect.height()), 1, logicalViewportSize.height()),
    };
}
}

namespace kiriview {
std::shared_ptr<SvgDisplaySource> SvgDisplaySource::open(
    const QByteArray& data, QString* errorString)
{
    const RustSvgImageSize intrinsicSize = rustSvgIntrinsicSize(Bridge::rustBytes(data));
    if (!intrinsicSize.valid) {
        return {};
    }

    const QSize imageSize(intrinsicSize.width, intrinsicSize.height);
    if (imageSize.isEmpty()) {
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::DetermineSvgImageSize));
        return {};
    }

    return std::make_shared<SvgDisplaySource>(data, imageSize);
}

SvgDisplaySource::SvgDisplaySource(QByteArray data, QSize imageSize)
    : m_data(std::move(data))
    , m_imageSize(imageSize)
{
}

QSize SvgDisplaySource::imageSize() const { return m_imageSize; }

StaticImageFirstDisplayDecodeResult SvgDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context) const
{
    if (!context.isValid()) {
        return {};
    }

    const QSize previewSize = svgFirstDisplayPreviewSize(m_imageSize, context.logicalViewportSize);
    if (previewSize.isEmpty()) {
        return {};
    }

    QImage preview = renderSvgImage(m_data, previewSize);
    if (preview.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { { FirstDisplayImageDecodeStatus::Error, {} }, { message, message } };
    }

    return { { FirstDisplayImageDecodeStatus::Ready, std::move(preview) }, {} };
}

bool SvgDisplaySource::supportsRasterDisplayRefinement() const { return true; }

StaticImageDisplayDecodeResult SvgDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    const QImage image = renderSvgImage(m_data, rasterSize);
    if (image.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message } };
    }
    return { image, {} };
}

StaticImageDisplayDecodeResult SvgDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    const QSize previewSize = boundedPreviewSize(m_imageSize, maximumLongEdge);
    const QImage preview = renderSvgImage(m_data, previewSize);
    if (preview.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message } };
    }
    return { preview, {} };
}

qsizetype SvgDisplaySource::byteCost() const { return m_data.size(); }
}
