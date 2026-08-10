// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rawdecoder.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "imagedecodeworkspace.h"
#include "localization/imageerrortext.h"
#include "rendering/imagerendering.h"
#include "rendering/staticimagedisplaysourcehelpers_p.h"
#include "staticimagedecode.h"

#include <QColorSpace>
#include <QtGlobal>
#include <algorithm>
#include <cstddef>
#include <libraw/libraw.h>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <utility>

namespace {
using ProcessedRawImage
    = std::unique_ptr<libraw_processed_image_t, decltype(&LibRaw::dcraw_clear_mem)>;

void setRawDecodeError(QString* errorString, QString message)
{
    if (errorString != nullptr) {
        *errorString = std::move(message);
    }
}

void setRawDecodeFailure(
    QString* errorString, QString* diagnosticDetail, QString message, QString detail)
{
    setRawDecodeError(errorString, std::move(message));
    setRawDecodeError(diagnosticDetail, std::move(detail));
}

QString rawBackendMessage(int errorCode)
{
    QString message = kiriview::imageErrorText(kiriview::ImageErrorTextId::UnknownLibrawError);
    if (const char* rawMessage = LibRaw::strerror(errorCode); rawMessage != nullptr) {
        message = QString::fromUtf8(rawMessage);
    }
    return message;
}

QString rawDecodeErrorString(const QString& action, int errorCode)
{
    const QString message = rawBackendMessage(errorCode);
    return kiriview::rawDecodeErrorText(action, message);
}

QString rawDecodeDiagnosticDetail(const QString& action, int errorCode)
{
    return QStringLiteral("LibRaw %1 failed with code %2: %3")
        .arg(action, QString::number(errorCode), rawBackendMessage(errorCode));
}

QString rawDecodeDiagnosticDetail(const QString& stage, const QString& message)
{
    return QStringLiteral("RAW %1 failed: %2").arg(stage, message);
}

QSize libRawImageSize(const LibRaw& processor)
{
    const libraw_image_sizes_t& sizes = processor.imgdata.sizes;
    if (sizes.iwidth > 0 && sizes.iheight > 0) {
        return QSize(sizes.iwidth, sizes.iheight);
    }
    return QSize(sizes.width, sizes.height);
}

bool validateRawImageSize(QSize size, QString* errorString, QString* diagnosticDetail)
{
    if (size.isEmpty()) {
        const QString message
            = kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageSizeInvalid);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(QStringLiteral("image size validation"), message));
        return false;
    }

    if (kiriview::estimatedRgbaByteCost(size) > kiriview::imageFullDecodeFallbackByteLimit) {
        const QString message
            = kiriview::imageErrorText(kiriview::ImageErrorTextId::RawFullDecodeTooLarge);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(QStringLiteral("full decode size guard"), message));
        return false;
    }

    return true;
}

std::optional<qsizetype> rawInitialRasterPeakByteCost(QSize imageSize)
{
    const qsizetype fullRasterByteCost = kiriview::estimatedRgbaByteCost(imageSize);
    const qsizetype previewByteCost = kiriview::estimatedRgbaByteCost(
        kiriview::boundedPreviewSize(imageSize, kiriview::imageBlockingDisplayLongEdgeMax));
    if (fullRasterByteCost <= 0 || previewByteCost <= 0) {
        return std::nullopt;
    }

    // The processed LibRaw bitmap, the application RGBA copy, and display-format conversion may
    // overlap. Once LibRaw retires, the retained full raster overlaps the scale output and its
    // working raster. Codec-private storage remains subject to LibRaw's independent cap.
    const qsizetype rawConversionPeak = kiriview::saturatedQtByteProduct(fullRasterByteCost, 3);
    const qsizetype previewProductionPeak = kiriview::saturatedQtByteSum(
        fullRasterByteCost, kiriview::saturatedQtByteProduct(previewByteCost, 2));
    const qsizetype peakByteCost = std::max(rawConversionPeak, previewProductionPeak);
    return peakByteCost == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(peakByteCost);
}

QImage rawDisplayImage(const QImage& source, QSize rasterSize)
{
    if (source.size() == rasterSize) {
        // Keep the source raster and the published raster as distinct physical allocations so
        // their independently retained admission charges never describe the same pixels.
        return source.copy();
    }
    return kiriview::scaledDisplayImage(source, rasterSize);
}

void setRawWorkspaceFailure(QString* errorString, QString* diagnosticDetail)
{
    const QString userMessage = kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData);
    setRawDecodeFailure(errorString, diagnosticDetail, userMessage,
        rawDecodeDiagnosticDetail(QStringLiteral("decoded-memory admission"),
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()));
}

std::optional<QImage> qImageFromRawProcessedImage(const libraw_processed_image_t* processedImage,
    QString* errorString, QString* diagnosticDetail, bool* resourceExhausted)
{
    if (processedImage == nullptr) {
        const QString message
            = kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageInvalid);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(QStringLiteral("processed image validation"), message));
        return std::nullopt;
    }

    if (processedImage->type != LIBRAW_IMAGE_BITMAP || processedImage->bits != 8
        || (processedImage->colors != 3 && processedImage->colors != 4)) {
        const QString message = kiriview::imageErrorText(
            kiriview::ImageErrorTextId::RawDecodedPixelFormatUnsupported);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(
                QStringLiteral("processed pixel format validation"), message));
        return std::nullopt;
    }

    const QSize imageSize(processedImage->width, processedImage->height);
    if (!validateRawImageSize(imageSize, errorString, diagnosticDetail)) {
        return std::nullopt;
    }

    const std::size_t channelCount = processedImage->colors;
    const std::size_t minimumDataSize = static_cast<std::size_t>(processedImage->width)
        * static_cast<std::size_t>(processedImage->height) * channelCount;
    if (processedImage->data_size < minimumDataSize) {
        const QString message
            = kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedPixelDataInvalid);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(QStringLiteral("processed pixel data validation"), message));
        return std::nullopt;
    }

    QImage image(imageSize, QImage::Format_RGBA8888);
    if (image.isNull()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        const QString message
            = kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageAllocationFailed);
        setRawDecodeFailure(errorString, diagnosticDetail, message,
            rawDecodeDiagnosticDetail(QStringLiteral("display image allocation"), message));
        return std::nullopt;
    }

    const unsigned char* source = processedImage->data;
    for (int y = 0; y < image.height(); ++y) {
        unsigned char* target = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const std::size_t sourceOffset
                = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width())
                      + static_cast<std::size_t>(x))
                * channelCount;
            const int targetOffset = x * 4;
            target[targetOffset] = source[sourceOffset];
            target[targetOffset + 1] = source[sourceOffset + 1];
            target[targetOffset + 2] = source[sourceOffset + 2];
            target[targetOffset + 3] = channelCount == 4 ? source[sourceOffset + 3] : 255;
        }
    }
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));

    return kiriview::displayReadyImage(image);
}

struct RawImageProduction
{
    std::optional<QImage> image;
    kiriview::ImageDecodeWorkspaceLease producerLease;
    bool resourceExhausted = false;
};

RawImageProduction decodeRawImage(const QByteArray& data,
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& workspaceBudget,
    QString* errorString, QString* diagnosticDetail)
{
    RawImageProduction result;
    LibRaw processor;
    int errorCode = processor.open_buffer(data.constData(), static_cast<std::size_t>(data.size()));
    if (errorCode != LIBRAW_SUCCESS) {
        const QString action
            = kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::ReadRawImage);
        setRawDecodeFailure(errorString, diagnosticDetail, rawDecodeErrorString(action, errorCode),
            rawDecodeDiagnosticDetail(action, errorCode));
        result.resourceExhausted = errorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    const QSize imageSize = libRawImageSize(processor);
    if (!validateRawImageSize(imageSize, errorString, diagnosticDetail)) {
        return result;
    }
    const std::optional<qsizetype> peakByteCost = rawInitialRasterPeakByteCost(imageSize);
    if (!peakByteCost.has_value()) {
        setRawWorkspaceFailure(errorString, diagnosticDetail);
        result.resourceExhausted = true;
        return result;
    }
    result.producerLease = workspaceBudget->startLease();
    if (!result.producerLease.tryReserve(*peakByteCost)) {
        setRawWorkspaceFailure(errorString, diagnosticDetail);
        result.resourceExhausted = true;
        return result;
    }

    processor.imgdata.params.use_camera_wb = 1;
    processor.imgdata.params.output_color = 1;
    processor.imgdata.params.output_bps = 8;
    processor.imgdata.rawparams.max_raw_memory_mb = static_cast<unsigned>(
        kiriview::imageFullDecodeFallbackByteLimit / (qsizetype { 1 } * 1024 * 1024));

    errorCode = processor.unpack();
    if (errorCode != LIBRAW_SUCCESS) {
        const QString action
            = kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::UnpackRawImage);
        setRawDecodeFailure(errorString, diagnosticDetail, rawDecodeErrorString(action, errorCode),
            rawDecodeDiagnosticDetail(action, errorCode));
        result.resourceExhausted = errorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    errorCode = processor.dcraw_process();
    if (errorCode != LIBRAW_SUCCESS) {
        const QString action
            = kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::ProcessRawImage);
        setRawDecodeFailure(errorString, diagnosticDetail, rawDecodeErrorString(action, errorCode),
            rawDecodeDiagnosticDetail(action, errorCode));
        result.resourceExhausted = errorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    int memImageErrorCode = LIBRAW_SUCCESS;
    ProcessedRawImage processedImage(
        processor.dcraw_make_mem_image(&memImageErrorCode), &LibRaw::dcraw_clear_mem);
    if (memImageErrorCode != LIBRAW_SUCCESS) {
        const QString action
            = kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::CreateDisplayImage);
        setRawDecodeFailure(errorString, diagnosticDetail,
            rawDecodeErrorString(action, memImageErrorCode),
            rawDecodeDiagnosticDetail(action, memImageErrorCode));
        result.resourceExhausted = memImageErrorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    result.image = qImageFromRawProcessedImage(
        processedImage.get(), errorString, diagnosticDetail, &result.resourceExhausted);
    return result;
}

class RawStaticImageDisplaySource final : public kiriview::StaticImageDisplaySource
{
public:
    explicit RawStaticImageDisplaySource(QImage image)
        : m_image(std::move(image))
    {
    }

    ~RawStaticImageDisplaySource() override = default;

    [[nodiscard]] QSize imageSize() const override { return m_image.size(); }
    [[nodiscard]] qsizetype byteCost() const override { return kiriview::imageByteCost(m_image); }
    [[nodiscard]] qsizetype retainedRasterByteCost() const override
    {
        return kiriview::imageByteCost(m_image);
    }
    [[nodiscard]] std::optional<qsizetype> initialDisplayDecodePeakByteCost(
        const kiriview::ImageFirstDisplayDecodeContext&, int) const override
    {
        const std::optional<qsizetype> totalPeakByteCost
            = rawInitialRasterPeakByteCost(m_image.size());
        const qsizetype retainedByteCost = retainedRasterByteCost();
        return totalPeakByteCost.has_value() && *totalPeakByteCost > retainedByteCost
            ? std::optional<qsizetype>(*totalPeakByteCost - retainedByteCost)
            : std::nullopt;
    }
    [[nodiscard]] bool supportsRasterDisplayRefinement() const override { return true; }
    [[nodiscard]] std::optional<qsizetype> rasterDisplayRefinementPeakByteCost(
        const QSize& rasterSize) const override
    {
        const qsizetype outputByteCost = kiriview::estimatedRgbaByteCost(rasterSize);
        return outputByteCost > 0
            ? std::optional<qsizetype>(kiriview::saturatedQtByteProduct(outputByteCost, 2))
            : std::nullopt;
    }

    [[nodiscard]] kiriview::StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const override
    {
        if (rasterSize.isEmpty()) {
            return {};
        }

        QImage image = rawDisplayImage(m_image, rasterSize);
        const bool resourceExhausted = image.isNull();
        return { std::move(image), {},
            resourceExhausted ? kiriview::StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : kiriview::StaticImageDisplayDecodeFailureCause::Decode };
    }

    [[nodiscard]] kiriview::StaticImageDisplayDecodeResult decodeBlockingDisplayImage(
        int maximumLongEdge) const override
    {
        const QSize rasterSize = kiriview::boundedPreviewSize(m_image.size(), maximumLongEdge);
        QImage image = rawDisplayImage(m_image, rasterSize);
        const bool resourceExhausted = image.isNull() && !rasterSize.isEmpty();
        return { std::move(image), {},
            resourceExhausted ? kiriview::StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : kiriview::StaticImageDisplayDecodeFailureCause::Decode };
    }

private:
    QImage m_image;
    Q_DISABLE_COPY_MOVE(RawStaticImageDisplaySource)
};
}

namespace kiriview {
DecodedImageResult decodeRawImageData(const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    QString errorString;
    QString diagnosticDetail;
    RawImageProduction production;
    try {
        production = decodeRawImage(data, workspaceBudget, &errorString, &diagnosticDetail);
    } catch (const std::bad_alloc&) {
        setRawWorkspaceFailure(&errorString, &diagnosticDetail);
        production.resourceExhausted = true;
    }
    if (!production.image.has_value()) {
        return failedDecodedImageResult(DecodedImageFailure {
            std::move(errorString),
            DecodedImageFailureRoute::Raw,
            DecodedImageFailureOperation::DecodeRawImage,
            std::move(diagnosticDetail),
            DecodedImageFailureSeverity::Error,
            false,
            production.resourceExhausted ? DecodedImageFailureCause::ResourceLimitExceeded
                                         : DecodedImageFailureCause::Unknown,
        });
    }

    std::shared_ptr<StaticImageDisplaySource> source
        = std::make_shared<RawStaticImageDisplaySource>(std::move(*production.image));
    ImageDecodeWorkspaceHold retainedSourceWorkspace
        = production.producerLease.splitRetained(source->retainedRasterByteCost());
    if (!retainedSourceWorkspace.isManaged()) {
        return failedDecodedImageResult(DecodedImageFailure {
            imageErrorText(ImageErrorTextId::ReadImageData),
            DecodedImageFailureRoute::Raw,
            DecodedImageFailureOperation::DecodeRawImage,
            QStringLiteral("RAW retained raster admission split failed"),
            DecodedImageFailureSeverity::Error,
            false,
            DecodedImageFailureCause::ResourceLimitExceeded,
        });
    }
    source->retainRasterOutputWorkspace(std::move(retainedSourceWorkspace));
    return staticDecodedImageResult(std::move(source), request, &errorString,
        std::move(workspaceBudget), std::move(production.producerLease));
}
}
