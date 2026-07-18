// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "qimagereaderdisplaysource.h"

#include "decoding/bufferedimagereader.h"
#include "imagerendering.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <QImageIOHandler>
#include <QTransform>
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

void appendDisplayDecodeFailure(kiriview::QImageReaderDisplayDecodeDiagnostics* diagnostics,
    kiriview::QImageReaderDisplayDecodeOperation operation, const QString& errorString)
{
    if (diagnostics == nullptr) {
        return;
    }

    const QString message = errorString.isEmpty() ? imageDataReadError() : errorString;
    diagnostics->failures.push_back(kiriview::QImageReaderDisplayDecodeFailure {
        operation,
        message,
        message,
    });
}

template <typename ConfigureReader>
QImage readBufferedImage(const QByteArray& data, const QByteArray& format, bool autoTransform,
    ConfigureReader configureReader, QString* errorString)
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

    return kiriview::displayReadyImage(image);
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
    const QSize imageSize = transformedImageSize(reader.size(), transformations);
    if (imageSize.isEmpty()) {
        setStaticImageDisplaySourceError(errorString, reader.errorString());
        return {};
    }

    return std::make_shared<QImageReaderDisplaySource>(
        data, format, imageSize, StaticImageReaderTransform { transformations });
}

QImageReaderDisplaySource::QImageReaderDisplaySource(
    QByteArray data, QByteArray format, QSize imageSize, StaticImageReaderTransform transform)
    : m_data(std::move(data))
    , m_format(std::move(format))
    , m_imageSize(imageSize)
    , m_transform(transform)
{
}

QSize QImageReaderDisplaySource::imageSize() const { return m_imageSize; }

QImageReaderFirstDisplayDecodeResult
QImageReaderDisplaySource::decodeFirstDisplayImageWithDiagnostics(
    const ImageFirstDisplayDecodeContext& context) const
{
    QImageReaderFirstDisplayDecodeResult result;
    if (!context.isValid() || !supportsJpegScaledFirstDisplay()) {
        return result;
    }

    const QSize scaledSize = firstDisplayScaledImageSize(m_imageSize, context.physicalViewportSize);
    if (scaledSize.isEmpty()) {
        return result;
    }

    QString errorString;
    QImage image = readScaledImage(scaledSize, &errorString);
    if (image.isNull()) {
        appendDisplayDecodeFailure(&result.diagnostics,
            QImageReaderDisplayDecodeOperation::FirstDisplayImage, errorString);
        result.firstDisplay.status = FirstDisplayImageDecodeStatus::Error;
        return result;
    }

    const qreal displayPixelsPerSourcePixel = imagePixelsPerSourcePixel(m_imageSize, image.size());
    if (displayPixelsPerSourcePixel <= 0.0) {
        appendDisplayDecodeFailure(&result.diagnostics,
            QImageReaderDisplayDecodeOperation::FirstDisplayImage,
            imageErrorText(ImageErrorTextId::DetermineJpegFirstDisplaySize));
        result.firstDisplay.status = FirstDisplayImageDecodeStatus::Error;
        return result;
    }

    result.firstDisplay
        = { FirstDisplayImageDecodeStatus::Ready, std::move(image), displayPixelsPerSourcePixel };
    return result;
}

FirstDisplayImageDecodeResult QImageReaderDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context, QString* errorString) const
{
    QImageReaderFirstDisplayDecodeResult result = decodeFirstDisplayImageWithDiagnostics(context);
    if (result.firstDisplay.status == FirstDisplayImageDecodeStatus::Error
        && !result.diagnostics.failures.empty()) {
        setStaticImageDisplaySourceError(errorString, result.diagnostics.userMessage());
    }
    return std::move(result.firstDisplay);
}

bool QImageReaderDisplaySource::supportsRasterDisplayRefinement() const { return true; }

QImageReaderDisplayDecodeResult QImageReaderDisplaySource::decodeRasterDisplayImageWithDiagnostics(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }

    return readScaledDisplayImage(
        rasterSize, QImageReaderDisplayDecodeOperation::RasterDisplayImage);
}

QImage QImageReaderDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize, QString* errorString) const
{
    QImageReaderDisplayDecodeResult result = decodeRasterDisplayImageWithDiagnostics(rasterSize);
    if (result.image.isNull() && !result.diagnostics.failures.empty()) {
        setStaticImageDisplaySourceError(errorString, result.diagnostics.userMessage());
    }
    return std::move(result.image);
}

QImageReaderDisplayDecodeResult
QImageReaderDisplaySource::decodeBlockingDisplayImageWithDiagnostics(int maximumLongEdge) const
{
    return readScaledDisplayImage(boundedPreviewSize(m_imageSize, maximumLongEdge),
        QImageReaderDisplayDecodeOperation::BlockingDisplayImage);
}

QImage QImageReaderDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge, QString* errorString) const
{
    QImageReaderDisplayDecodeResult result
        = decodeBlockingDisplayImageWithDiagnostics(maximumLongEdge);
    if (result.image.isNull() && !result.diagnostics.failures.empty()) {
        setStaticImageDisplaySourceError(errorString, result.diagnostics.userMessage());
    }
    return std::move(result.image);
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

QImageReaderDisplayDecodeResult QImageReaderDisplaySource::readScaledDisplayImage(
    QSize scaledSize, QImageReaderDisplayDecodeOperation operation) const
{
    QImageReaderDisplayDecodeResult result;
    QString errorString;
    result.image = readScaledImage(scaledSize, &errorString);
    if (result.image.isNull()) {
        appendDisplayDecodeFailure(&result.diagnostics, operation, errorString);
    }
    return result;
}

QImage QImageReaderDisplaySource::readScaledImage(QSize scaledSize, QString* errorString) const
{
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
        errorString);
    if (image.isNull() || !hasTransform) {
        return image;
    }
    return displayReadyImage(transformedImage(std::move(image), m_transform.transformations));
}

}
