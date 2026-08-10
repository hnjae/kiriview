// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "svgdisplaysource.h"

#include "bridge/rustqtconversion.h"
#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
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
QImage imageFromPremultipliedRgbaBytes(const QByteArray& bytes, QSize size, bool* resourceExhausted)
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

    QImage image = kiriview::copiedImageFromBytes(bytes, size,
        static_cast<qsizetype>(size.width()) * 4, QImage::Format_RGBA8888_Premultiplied);
    if (image.isNull() && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return image;
}

QByteArray renderSvgImageBytes(const QByteArray& data, QSize size)
{
    if (size.isEmpty()) {
        return {};
    }

    return kiriview::Bridge::qtByteArray(kiriview::rustRenderSvgImage(
        kiriview::Bridge::rustBytes(data), size.width(), size.height()));
}

QImage renderSvgImage(const QByteArray& data, QSize size, bool* resourceExhausted = nullptr)
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    return imageFromPremultipliedRgbaBytes(
        renderSvgImageBytes(data, size), size, resourceExhausted);
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

StaticImageSourceDetailModel SvgDisplaySource::detailModel() const
{
    return StaticImageSourceDetailModel::ScalableRasterization;
}

std::optional<qsizetype> SvgDisplaySource::initialDisplayDecodePeakByteCost(
    const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const
{
    if (!context.isValid()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }

    const QSize firstDisplaySize
        = svgFirstDisplayPreviewSize(m_imageSize, context.logicalViewportSize);
    if (firstDisplaySize.isEmpty()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }
    return rasterDisplayRefinementPeakByteCost(firstDisplaySize);
}

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

std::optional<qsizetype> SvgDisplaySource::rasterDisplayRefinementPeakByteCost(
    const QSize& rasterSize) const
{
    const qsizetype outputByteCost = estimatedRgbaByteCost(rasterSize);
    return outputByteCost > 0 ? std::optional<qsizetype>(saturatedQtByteProduct(outputByteCost, 2))
                              : std::nullopt;
}

StaticImageDisplayDecodeResult SvgDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    bool resourceExhausted = false;
    const QImage image = renderSvgImage(m_data, rasterSize, &resourceExhausted);
    if (image.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message },
            resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : StaticImageDisplayDecodeFailureCause::Decode };
    }
    return { image, {} };
}

StaticImageDisplayDecodeResult SvgDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    const QSize previewSize = boundedPreviewSize(m_imageSize, maximumLongEdge);
    bool resourceExhausted = false;
    const QImage preview = renderSvgImage(m_data, previewSize, &resourceExhausted);
    if (preview.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message },
            resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : StaticImageDisplayDecodeFailureCause::Decode };
    }
    return { preview, {} };
}

qsizetype SvgDisplaySource::byteCost() const { return m_data.size(); }
}
