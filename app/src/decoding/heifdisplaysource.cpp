// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifdisplaysource.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/heifcontainer.h"
#include "decoding/heifsupport.h"
#include "decoding/imagedecodeworkspace.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <libheif/heif_tiling.h>

#include <QPainter>
#include <QRectF>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
bool heifResourceFailure(heif_error error)
{
    return error.code == heif_error_Memory_allocation_error
        || error.subcode == heif_suberror_Security_limit_exceeded;
}

std::optional<qsizetype> heifDecoderByteLimit(QSize decodeSize)
{
    const qsizetype rgbaByteCost = kiriview::estimatedRgbaByteCost(decodeSize);
    if (rgbaByteCost <= 0) {
        return std::nullopt;
    }
    return std::max(kiriview::heifDisplaySourceOpenWorkspaceByteCount,
        kiriview::saturatedQtByteProduct(rgbaByteCost, 8));
}

std::optional<qsizetype> heifFullDecodePeakByteCost(QSize sourceSize, QSize targetSize)
{
    const qsizetype sourceByteCost = kiriview::estimatedRgbaByteCost(sourceSize);
    const qsizetype targetByteCost = kiriview::estimatedRgbaByteCost(targetSize);
    const std::optional<qsizetype> decoderByteLimit = heifDecoderByteLimit(sourceSize);
    if (sourceByteCost <= 0 || targetByteCost <= 0 || !decoderByteLimit.has_value()) {
        return std::nullopt;
    }
    const qsizetype applicationPeak = std::max(kiriview::saturatedQtByteProduct(sourceByteCost, 2),
        kiriview::saturatedQtByteSum(sourceByteCost, targetByteCost));
    return kiriview::saturatedQtByteSum(*decoderByteLimit, applicationPeak);
}

std::optional<kiriview::HeifPrimaryImage> openHeifPrimaryImageWithMemoryLimit(
    const QByteArray& data, qsizetype maximumTotalMemory, QString* errorString,
    bool* resourceExhausted)
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    if (maximumTotalMemory <= 0) {
        return std::nullopt;
    }

    bool contextResourceExhausted = false;
    std::optional<kiriview::HeifContext> context = kiriview::openHeifContext(data, errorString,
        kiriview::HeifContextOpenLimits { static_cast<std::uint64_t>(maximumTotalMemory) },
        &contextResourceExhausted);
    if (!context.has_value()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = contextResourceExhausted;
        }
        return std::nullopt;
    }

    kiriview::HeifImageHandle handle;
    const heif_error error = heif_context_get_primary_image_handle(context->get(), handle.out());
    if (error.code != heif_error_Ok) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = heifResourceFailure(error);
        }
        kiriview::setStaticImageDisplaySourceError(errorString,
            kiriview::heifErrorString(
                kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::ReadPrimaryImage),
                error));
        return std::nullopt;
    }
    return kiriview::HeifPrimaryImage { std::move(*context), std::move(handle) };
}
}

namespace kiriview {
HeifDisplaySource::HeifDisplaySource(
    QByteArray data, QSize imageSize, std::optional<HeifTileGrid> tileGrid)
    : m_data(std::move(data))
    , m_imageSize(imageSize)
    , m_tileGrid(tileGrid)
{
}

QSize HeifDisplaySource::imageSize() const { return m_imageSize; }

qsizetype HeifDisplaySource::byteCost() const { return m_data.size(); }

std::optional<qsizetype> HeifDisplaySource::initialDisplayDecodePeakByteCost(
    const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const
{
    Q_UNUSED(context);
    return heifFullDecodePeakByteCost(
        m_imageSize, boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
}

bool HeifDisplaySource::supportsRasterDisplayRefinement() const { return true; }

std::optional<qsizetype> HeifDisplaySource::rasterDisplayRefinementPeakByteCost(
    const QSize& rasterSize) const
{
    const qsizetype targetByteCost = estimatedRgbaByteCost(rasterSize);
    if (targetByteCost <= 0) {
        return std::nullopt;
    }
    if (m_tileGrid.has_value()) {
        const qsizetype tileByteCost
            = estimatedRgbaByteCost(QSize(m_tileGrid->tileWidth, m_tileGrid->tileHeight));
        const std::optional<qsizetype> decoderByteLimit
            = heifDecoderByteLimit(QSize(m_tileGrid->tileWidth, m_tileGrid->tileHeight));
        if (tileByteCost <= 0 || !decoderByteLimit.has_value()) {
            return std::nullopt;
        }
        const qsizetype applicationPeak
            = saturatedQtByteSum(targetByteCost, saturatedQtByteProduct(tileByteCost, 2));
        return saturatedQtByteSum(*decoderByteLimit, applicationPeak);
    }

    return heifFullDecodePeakByteCost(m_imageSize, rasterSize);
}

StaticImageDisplayDecodeResult HeifDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }
    QString errorString;
    bool resourceExhausted = false;
    QImage image;
    if (m_tileGrid.has_value()) {
        image = decodeGridRasterDisplayImage(rasterSize, &errorString, &resourceExhausted);
    } else {
        image = decodeFullOrScaled(rasterSize, &errorString, &resourceExhausted);
    }
    return { std::move(image), { errorString, errorString },
        resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                          : StaticImageDisplayDecodeFailureCause::Decode };
}

StaticImageDisplayDecodeResult HeifDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    QString errorString;
    bool resourceExhausted = false;
    QImage image = decodeFullOrScaled(
        boundedPreviewSize(m_imageSize, maximumLongEdge), &errorString, &resourceExhausted);
    return { std::move(image), { errorString, errorString },
        resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                          : StaticImageDisplayDecodeFailureCause::Decode };
}

QImage HeifDisplaySource::decodeFullOrScaled(
    QSize targetSize, QString* errorString, bool* resourceExhausted) const
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    if (estimatedRgbaByteCost(m_imageSize) > imageFullDecodeFallbackByteLimit) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifFullDecodeFallbackTooLarge));
        return {};
    }

    const std::optional<qsizetype> decoderByteLimit = heifDecoderByteLimit(m_imageSize);
    if (!decoderByteLimit.has_value()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        return {};
    }
    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImageWithMemoryLimit(
        m_data, *decoderByteLimit, errorString, resourceExhausted);
    if (!opened.has_value()) {
        return {};
    }

    HeifDecodingOptions options;
    HeifImage heifImage;
    const heif_error error = heif_decode_image(opened->handle.get(), heifImage.out(),
        heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options.get());
    if (error.code != heif_error_Ok) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = heifResourceFailure(error);
        }
        setStaticImageDisplaySourceError(errorString,
            heifErrorString(
                imageErrorActionText(ImageErrorActionTextId::DecodePrimaryImage), error));
        return {};
    }

    HeifImageConversionFailureCause conversionFailure = HeifImageConversionFailureCause::Invalid;
    std::optional<QImage> image
        = qImageFromHeifImage(heifImage.get(), errorString, &conversionFailure);
    if (!image.has_value()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted
                = conversionFailure == HeifImageConversionFailureCause::ResourceLimitExceeded;
        }
        return {};
    }

    QImage displayImage
        = scaledDisplayImage(*image, targetSize.isEmpty() ? m_imageSize : targetSize);
    if (displayImage.isNull() && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return displayImage;
}

QImage HeifDisplaySource::decodeGridRasterDisplayImage(
    QSize rasterSize, QString* errorString, bool* resourceExhausted) const
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    if (!m_tileGrid.has_value()) {
        return {};
    }

    const std::optional<qsizetype> decoderByteLimit
        = heifDecoderByteLimit(QSize(m_tileGrid->tileWidth, m_tileGrid->tileHeight));
    if (!decoderByteLimit.has_value()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        return {};
    }
    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImageWithMemoryLimit(
        m_data, *decoderByteLimit, errorString, resourceExhausted);
    if (!opened.has_value()) {
        return {};
    }

    QImage image(rasterSize, QImage::Format_RGBA8888_Premultiplied);
    if (image.isNull()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifDecodedImageAllocationFailed));
        return {};
    }
    image.fill(Qt::transparent);

    const qreal scaleX = static_cast<qreal>(rasterSize.width()) / m_imageSize.width();
    const qreal scaleY = static_cast<qreal>(rasterSize.height()) / m_imageSize.height();
    HeifDecodingOptions options;
    QPainter painter(&image);
    if (!painter.isActive()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifDecodedImageAllocationFailed));
        return {};
    }
    for (const HeifTileDecodeRegion& region :
        heifTileDecodeRegions(*m_tileGrid, QRect(QPoint(0, 0), m_imageSize))) {
        HeifImage heifImage;
        const heif_error error = heif_image_handle_decode_image_tile(opened->handle.get(),
            heifImage.out(), heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options.get(),
            static_cast<std::uint32_t>(region.tileX), static_cast<std::uint32_t>(region.tileY));
        if (error.code != heif_error_Ok) {
            if (resourceExhausted != nullptr) {
                *resourceExhausted = heifResourceFailure(error);
            }
            setStaticImageDisplaySourceError(errorString,
                heifErrorString(
                    imageErrorActionText(ImageErrorActionTextId::DecodeHeifGridTile), error));
            return {};
        }

        HeifImageConversionFailureCause conversionFailure
            = HeifImageConversionFailureCause::Invalid;
        std::optional<QImage> tileImage
            = qImageFromHeifImage(heifImage.get(), errorString, &conversionFailure);
        if (!tileImage.has_value()) {
            if (resourceExhausted != nullptr) {
                *resourceExhausted
                    = conversionFailure == HeifImageConversionFailureCause::ResourceLimitExceeded;
            }
            return {};
        }

        painter.drawImage(QRectF(region.targetPoint.x() * scaleX, region.targetPoint.y() * scaleY,
                              tileImage->width() * scaleX, tileImage->height() * scaleY),
            *tileImage);
    }

    return image;
}

std::shared_ptr<HeifDisplaySource> openHeifDisplaySource(const QByteArray& data,
    QString* errorString, std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    qsizetype perOperationBaselineByteCount, bool* resourceExhausted)
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    if (!isLikelyHeifStillImageContainer(data)) {
        return {};
    }
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    ImageDecodeWorkspaceLease openWorkspace = ImageDecodeWorkspaceDetail::startLeaseForOperation(
        *workspaceBudget, perOperationBaselineByteCount);
    if (!ImageDecodeWorkspaceDetail::tryReserve(
            openWorkspace, heifDisplaySourceOpenWorkspaceByteCount)) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::ReadImageData));
        return {};
    }

    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImageWithMemoryLimit(
        data, heifDisplaySourceOpenWorkspaceByteCount, errorString, resourceExhausted);
    if (!opened.has_value()) {
        return {};
    }

    const QSize imageSize(heif_image_handle_get_width(opened->handle.get()),
        heif_image_handle_get_height(opened->handle.get()));
    if (imageSize.isEmpty()) {
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifImageSizeInvalid));
        return {};
    }

    heif_image_tiling tiling {};
    tiling.version = 1;
    const heif_error error = heif_image_handle_get_image_tiling(opened->handle.get(), 1, &tiling);
    std::optional<HeifTileGrid> tileGrid;
    if (error.code == heif_error_Ok) {
        tileGrid = heifTileGridForTiling(tiling);
    }

    return std::make_shared<HeifDisplaySource>(data, imageSize, tileGrid);
}
}
