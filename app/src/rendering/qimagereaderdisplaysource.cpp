// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "qimagereaderdisplaysource.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/bufferedimagereader.h"
#include "imagerendering.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <QImageIOHandler>
#include <QPixelFormat>
#include <QTransform>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace {
QString imageDataReadError()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData);
}

QSize transformedImageSize(QSize size, QImageIOHandler::Transformations transformations)
{
    if (size.isEmpty()) {
        return {};
    }
    if (transformations.toInt() & QImageIOHandler::TransformationRotate90) {
        return size.transposed();
    }
    return size;
}

qsizetype estimatedAlignedPixelByteCost(QSize size, qsizetype bitsPerPixel)
{
    if (size.isEmpty() || bitsPerPixel <= 0) {
        return 0;
    }

    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    const qsizetype width = size.width();
    const qsizetype height = size.height();
    if (width > (maximum - 31) / bitsPerPixel) {
        return maximum;
    }
    const qsizetype bytesPerLine = ((width * bitsPerPixel + 31) / 32) * 4;
    return bytesPerLine > maximum / height ? maximum : bytesPerLine * height;
}

qsizetype estimatedAlignedImageByteCost(QSize size, QImage::Format format)
{
    constexpr qsizetype fallbackBitsPerPixel = 128;
    const qsizetype reportedBitsPerPixel = QImage::toPixelFormat(format).bitsPerPixel();
    return estimatedAlignedPixelByteCost(
        size, reportedBitsPerPixel > 0 ? reportedBitsPerPixel : fallbackBitsPerPixel);
}

qsizetype smoothScaleWorkingBitsPerPixel(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGBX32FPx4:
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
    case QImage::Format_Invalid:
        return 128;
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
    case QImage::Format_Grayscale16:
        return 64;
    default:
        return 32;
    }
}

QImage transformedImage(QImage image, QImageIOHandler::Transformations transformations)
{
    if (image.isNull() || transformations == QImageIOHandler::TransformationNone) {
        return image;
    }

    switch (static_cast<QImageIOHandler::Transformation>(transformations.toInt())) {
    case QImageIOHandler::TransformationNone:
        return image;
    case QImageIOHandler::TransformationMirror:
        return image.flipped(Qt::Horizontal);
    case QImageIOHandler::TransformationFlip:
        return image.flipped(Qt::Vertical);
    case QImageIOHandler::TransformationRotate180:
        return image.transformed(QTransform().rotate(180));
    case QImageIOHandler::TransformationRotate90:
        return image.transformed(QTransform().rotate(90));
    case QImageIOHandler::TransformationMirrorAndRotate90:
        return image.flipped(Qt::Horizontal).transformed(QTransform().rotate(90));
    case QImageIOHandler::TransformationFlipAndRotate90:
        return image.flipped(Qt::Vertical).transformed(QTransform().rotate(90));
    case QImageIOHandler::TransformationRotate270:
        return image.transformed(QTransform().rotate(270));
    }

    return image;
}

void appendDisplayDecodeFailure(
    kiriview::StaticImageDisplayDecodeDiagnostics* diagnostics, const QString& errorString)
{
    if (diagnostics == nullptr) {
        return;
    }

    const QString message = errorString.isEmpty() ? imageDataReadError() : errorString;
    diagnostics->userMessage = message;
    diagnostics->diagnosticDetail = message;
}

template <typename ConfigureReader>
QImage readBufferedImage(const QByteArray& data, const QByteArray& format, bool autoTransform,
    ConfigureReader configureReader, QString* errorString, bool* resourceExhausted)
{
    kiriview::BufferedImageReader reader(data, format, autoTransform);
    if (!reader) {
        kiriview::setStaticImageDisplaySourceError(errorString, imageDataReadError());
        return {};
    }

    configureReader(reader);
    QImage image = reader.read();
    if (image.isNull()) {
        kiriview::setStaticImageDisplaySourceError(errorString, reader.errorString());
        return {};
    }

    QImage displayImage = kiriview::displayReadyImage(image);
    if (displayImage.isNull() && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return displayImage;
}
}

namespace kiriview {
std::shared_ptr<QImageReaderDisplaySource> QImageReaderDisplaySource::open(
    const QByteArray& data, const QByteArray& format, QString* errorString)
{
    BufferedImageReader reader(data, format);
    if (!reader) {
        setStaticImageDisplaySourceError(errorString, imageDataReadError());
        return {};
    }

    const QImageIOHandler::Transformations transformations = reader.transformation();
    const QSize readerImageSize = reader.size();
    const QSize imageSize = transformedImageSize(readerImageSize, transformations);
    if (imageSize.isEmpty()) {
        setStaticImageDisplaySourceError(errorString, reader.errorString());
        return {};
    }

    return std::make_shared<QImageReaderDisplaySource>(data, format, imageSize,
        StaticImageReaderTransform { transformations }, readerImageSize,
        reader.supportsOption(QImageIOHandler::ScaledSize), reader.imageFormat());
}

QImageReaderDisplaySource::QImageReaderDisplaySource(QByteArray data, QByteArray format,
    QSize imageSize, StaticImageReaderTransform transform, QSize readerImageSize,
    bool readerSupportsScaledSize, QImage::Format readerImageFormat)
    : m_data(std::move(data))
    , m_format(std::move(format))
    , m_imageSize(imageSize)
    , m_transform(transform)
    , m_readerImageSize(readerImageSize.isEmpty()
              ? transformedImageSize(imageSize, transform.transformations)
              : readerImageSize)
    , m_readerSupportsScaledSize(readerSupportsScaledSize)
    , m_readerImageFormat(readerImageFormat)
{
}

QSize QImageReaderDisplaySource::imageSize() const { return m_imageSize; }

std::optional<qsizetype> QImageReaderDisplaySource::initialDisplayDecodePeakByteCost(
    const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const
{
    if (!context.isValid() || !supportsJpegScaledFirstDisplay()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }

    const QSize firstDisplaySize
        = firstDisplayScaledImageSize(m_imageSize, context.logicalViewportSize);
    if (firstDisplaySize.isEmpty()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }
    return rasterDisplayRefinementPeakByteCost(firstDisplaySize);
}

StaticImageFirstDisplayDecodeResult QImageReaderDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context) const
{
    StaticImageFirstDisplayDecodeResult result;
    if (!context.isValid() || !supportsJpegScaledFirstDisplay()) {
        return result;
    }

    const QSize scaledSize = firstDisplayScaledImageSize(m_imageSize, context.logicalViewportSize);
    if (scaledSize.isEmpty()) {
        return result;
    }

    QString errorString;
    QImage image = readScaledImage(scaledSize, &errorString);
    if (image.isNull()) {
        appendDisplayDecodeFailure(&result.diagnostics, errorString);
        result.firstDisplay.status = FirstDisplayImageDecodeStatus::Error;
        return result;
    }

    result.firstDisplay = { FirstDisplayImageDecodeStatus::Ready, std::move(image) };
    return result;
}

bool QImageReaderDisplaySource::supportsRasterDisplayRefinement() const { return true; }

std::optional<qsizetype> QImageReaderDisplaySource::rasterDisplayRefinementPeakByteCost(
    const QSize& rasterSize) const
{
    const qsizetype outputByteCost = estimatedRgbaByteCost(rasterSize);
    if (outputByteCost <= 0) {
        return std::nullopt;
    }

    const bool hasTransform = m_transform.hasTransform();
    const QSize readerRasterSize
        = hasTransform ? transformedImageSize(rasterSize, m_transform.transformations) : rasterSize;
    const qsizetype decodedRasterByteCost
        = estimatedAlignedImageByteCost(readerRasterSize, m_readerImageFormat);
    if (decodedRasterByteCost <= 0) {
        return std::nullopt;
    }

    const qsizetype decodedSourceByteCost
        = estimatedAlignedImageByteCost(m_readerImageSize, m_readerImageFormat);
    if (decodedSourceByteCost <= 0) {
        return std::nullopt;
    }
    // ScaledSize only promises the requested result, not that a handler materializes no larger
    // intermediate raster. Use the full decoded source as the generic handler-independent upper
    // bound; a codec may provide a narrower estimate only when its peak is independently proven.
    qsizetype readerPeakByteCost = saturatedQtByteSum(decodedSourceByteCost, decodedRasterByteCost);
    if (!m_readerSupportsScaledSize) {
        const qsizetype workingBitsPerPixel = smoothScaleWorkingBitsPerPixel(m_readerImageFormat);
        const qsizetype workingSourceByteCost
            = estimatedAlignedPixelByteCost(m_readerImageSize, workingBitsPerPixel);
        const qsizetype workingRasterByteCost
            = estimatedAlignedPixelByteCost(readerRasterSize, workingBitsPerPixel);
        if (workingSourceByteCost <= 0 || workingRasterByteCost <= 0) {
            return std::nullopt;
        }
        // QImageReader's fallback smooth scale may keep the handler's full decoded raster while
        // converting a full-size working raster, producing the scaled working raster, and then
        // converting that result back to the handler format. Charge the conservative common
        // overlap because the handler contract exposes no narrower peak.
        readerPeakByteCost = saturatedQtByteSum(
            readerPeakByteCost, saturatedQtByteSum(workingSourceByteCost, workingRasterByteCost));
    }

    const qsizetype conversionPeakByteCost
        = saturatedQtByteSum(decodedRasterByteCost, outputByteCost);
    const bool compoundTransform
        = m_transform.transformations == QImageIOHandler::TransformationMirrorAndRotate90
        || m_transform.transformations == QImageIOHandler::TransformationFlipAndRotate90;
    const qsizetype transformPeakByteCost
        = saturatedQtByteProduct(outputByteCost, compoundTransform ? 3 : 2);
    return std::max({ readerPeakByteCost, conversionPeakByteCost, transformPeakByteCost });
}

StaticImageDisplayDecodeResult QImageReaderDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }

    return readScaledDisplayImage(rasterSize);
}

StaticImageDisplayDecodeResult QImageReaderDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    return readScaledDisplayImage(boundedPreviewSize(m_imageSize, maximumLongEdge));
}

qsizetype QImageReaderDisplaySource::byteCost() const { return m_data.size(); }

StaticImageReaderTransform QImageReaderDisplaySource::imageReaderTransform() const
{
    return m_transform;
}

bool QImageReaderDisplaySource::supportsJpegScaledFirstDisplay() const
{
    const QByteArray format = m_format.toLower();
    return format == QByteArrayLiteral("jpg") || format == QByteArrayLiteral("jpeg");
}

StaticImageDisplayDecodeResult QImageReaderDisplaySource::readScaledDisplayImage(
    QSize scaledSize) const
{
    StaticImageDisplayDecodeResult result;
    QString errorString;
    bool resourceExhausted = false;
    result.image = readScaledImage(scaledSize, &errorString, &resourceExhausted);
    if (result.image.isNull()) {
        appendDisplayDecodeFailure(&result.diagnostics, errorString);
        if (resourceExhausted) {
            result.failureCause = StaticImageDisplayDecodeFailureCause::ResourceExhausted;
        }
    }
    return result;
}

QImage QImageReaderDisplaySource::readScaledImage(
    QSize scaledSize, QString* errorString, bool* resourceExhausted) const
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    const bool hasTransform = m_transform.hasTransform();
    const QSize readerScaledSize
        = hasTransform ? transformedImageSize(scaledSize, m_transform.transformations) : scaledSize;
    QImage image = readBufferedImage(
        m_data, m_format, !hasTransform,
        [&readerScaledSize](BufferedImageReader& reader) {
            if (!readerScaledSize.isEmpty()) {
                reader.setScaledSize(readerScaledSize);
            }
        },
        errorString, resourceExhausted);
    if (image.isNull() || !hasTransform) {
        return image;
    }
    QImage transformed = transformedImage(std::move(image), m_transform.transformations);
    if (transformed.isNull()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        return {};
    }
    QImage displayImage = displayReadyImage(transformed);
    if (displayImage.isNull() && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return displayImage;
}

}
