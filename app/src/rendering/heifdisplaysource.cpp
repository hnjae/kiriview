// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifdisplaysource.h"

#include "cache/imagebytecost.h"
#include "decoding/heifcontainer.h"
#include "decoding/heifsupport.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <libheif/heif_tiling.h>

#include <QPainter>
#include <QRectF>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

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

bool HeifDisplaySource::supportsRasterDisplayRefinement() const { return true; }

StaticImageDisplayDecodeResult HeifDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }
    QString errorString;
    QImage image;
    if (m_tileGrid.has_value()) {
        image = decodeGridRasterDisplayImage(rasterSize, &errorString);
    } else {
        image = decodeFullOrScaled(rasterSize, &errorString);
    }
    return { std::move(image), { errorString, errorString } };
}

StaticImageDisplayDecodeResult HeifDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    QString errorString;
    QImage image
        = decodeFullOrScaled(boundedPreviewSize(m_imageSize, maximumLongEdge), &errorString);
    return { std::move(image), { errorString, errorString } };
}

QImage HeifDisplaySource::decodeFullOrScaled(QSize targetSize, QString* errorString) const
{
    if (estimatedRgbaByteCost(m_imageSize) > imageFullDecodeFallbackByteLimit) {
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifFullDecodeFallbackTooLarge));
        return {};
    }

    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImage(m_data, errorString);
    if (!opened.has_value()) {
        return {};
    }

    HeifDecodingOptions options;
    HeifImage heifImage;
    const heif_error error = heif_decode_image(opened->handle.get(), heifImage.out(),
        heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options.get());
    if (error.code != heif_error_Ok) {
        setStaticImageDisplaySourceError(errorString,
            heifErrorString(
                imageErrorActionText(ImageErrorActionTextId::DecodePrimaryImage), error));
        return {};
    }

    std::optional<QImage> image = qImageFromHeifImage(heifImage.get(), errorString);
    if (!image.has_value()) {
        return {};
    }

    return scaledDisplayImage(*image, targetSize.isEmpty() ? m_imageSize : targetSize);
}

QImage HeifDisplaySource::decodeGridRasterDisplayImage(QSize rasterSize, QString* errorString) const
{
    if (!m_tileGrid.has_value()) {
        return {};
    }

    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImage(m_data, errorString);
    if (!opened.has_value()) {
        return {};
    }

    QImage image(rasterSize, QImage::Format_RGBA8888_Premultiplied);
    if (image.isNull()) {
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::HeifDecodedImageAllocationFailed));
        return {};
    }
    image.fill(Qt::transparent);

    const qreal scaleX = static_cast<qreal>(rasterSize.width()) / m_imageSize.width();
    const qreal scaleY = static_cast<qreal>(rasterSize.height()) / m_imageSize.height();
    HeifDecodingOptions options;
    QPainter painter(&image);
    for (const HeifTileDecodeRegion& region :
        heifTileDecodeRegions(*m_tileGrid, QRect(QPoint(0, 0), m_imageSize))) {
        HeifImage heifImage;
        const heif_error error = heif_image_handle_decode_image_tile(opened->handle.get(),
            heifImage.out(), heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options.get(),
            static_cast<std::uint32_t>(region.tileX), static_cast<std::uint32_t>(region.tileY));
        if (error.code != heif_error_Ok) {
            setStaticImageDisplaySourceError(errorString,
                heifErrorString(
                    imageErrorActionText(ImageErrorActionTextId::DecodeHeifGridTile), error));
            return {};
        }

        std::optional<QImage> tileImage = qImageFromHeifImage(heifImage.get(), errorString);
        if (!tileImage.has_value()) {
            return {};
        }

        painter.drawImage(QRectF(region.targetPoint.x() * scaleX, region.targetPoint.y() * scaleY,
                              tileImage->width() * scaleX, tileImage->height() * scaleY),
            *tileImage);
    }

    return image;
}

std::shared_ptr<HeifDisplaySource> openHeifDisplaySource(
    const QByteArray& data, QString* errorString)
{
    if (!isLikelyHeifStillImageContainer(data)) {
        return {};
    }

    std::optional<HeifPrimaryImage> opened = openHeifPrimaryImage(data, errorString);
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
