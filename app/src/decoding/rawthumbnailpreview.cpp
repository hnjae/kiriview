// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rawthumbnailpreview.h"

#include "bufferedimagereader.h"
#include "cache/imagebytecost.h"
#include "decoding/imagerendering.h"
#include "imageinputclassification.h"
#include "localization/imageerrortext.h"
#include "location/sourcekey.h"
#include "rawdecoder.h"

#include <QColorSpace>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <libraw/libraw.h>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace {
using ProcessedRawImage
    = std::unique_ptr<libraw_processed_image_t, decltype(&LibRaw::dcraw_clear_mem)>;
constexpr qsizetype rawEmbeddedThumbnailPreviewRasterByteLimit = qsizetype { 64 } * 1024 * 1024;
constexpr qsizetype rawEmbeddedThumbnailPreviewPeakByteCount
    = kiriview::rawImageOpenWorkspaceByteCount + rawEmbeddedThumbnailPreviewRasterByteLimit * 3;

bool validImageSize(QSize size)
{
    return size.isValid() && !size.isEmpty() && size.width() > 0 && size.height() > 0;
}

void setError(QString* errorString, QString message)
{
    if (errorString != nullptr) {
        *errorString = std::move(message);
    }
}

QString rawDecodeErrorString(const QString& action, int errorCode)
{
    QString message = kiriview::imageErrorText(kiriview::ImageErrorTextId::UnknownLibrawError);
    if (const char* rawMessage = LibRaw::strerror(errorCode); rawMessage != nullptr) {
        message = QString::fromUtf8(rawMessage);
    }

    return kiriview::rawDecodeErrorText(action, message);
}

kiriview::RawEmbeddedThumbnailPreviewResult rawResult(
    kiriview::RawEmbeddedThumbnailPreviewStatus status, QImage image = {}, QSize originalSize = {},
    QString errorString = {})
{
    return kiriview::RawEmbeddedThumbnailPreviewResult {
        status,
        std::move(image),
        originalSize,
        std::move(errorString),
    };
}

bool rawThumbnailMissingError(int errorCode)
{
    return errorCode == LIBRAW_NO_THUMBNAIL || errorCode == LIBRAW_UNSUPPORTED_THUMBNAIL
        || errorCode == LIBRAW_REQUEST_FOR_NONEXISTENT_IMAGE;
}

bool rawInputIsEligibleForEmbeddedThumbnail(
    const QByteArray& data, const kiriview::ImageDecodeRequest& request)
{
    const kiriview::ImageInputClassification classification
        = kiriview::classifyImageInput(data, request.imageUrl().fileName());
    return classification.kind == kiriview::ImageInputKind::Raw;
}

QSize libRawImageSize(const LibRaw& processor)
{
    const libraw_image_sizes_t& sizes = processor.imgdata.sizes;
    if (sizes.iwidth > 0 && sizes.iheight > 0) {
        return QSize(sizes.iwidth, sizes.iheight);
    }
    return QSize(sizes.width, sizes.height);
}

bool rawPreviewByteCostFitsBudget(QSize size)
{
    const qsizetype byteCost = kiriview::estimatedRgbaByteCost(size);
    return byteCost > 0 && byteCost <= kiriview::imageFullDecodeFallbackByteLimit;
}

bool rawEmbeddedThumbnailRasterFitsAdmission(QSize size)
{
    const qsizetype byteCost = kiriview::estimatedRgbaByteCost(size);
    return byteCost > 0 && byteCost <= rawEmbeddedThumbnailPreviewRasterByteLimit;
}

bool validateTrustedOriginalSize(QSize size)
{
    return validImageSize(size) && rawPreviewByteCostFitsBudget(size);
}

unsigned rawMemoryLimitMiB(qsizetype byteLimit)
{
    constexpr qsizetype bytesPerMiB = qsizetype { 1024 } * 1024;
    const qsizetype roundedMiB = (byteLimit + bytesPerMiB - 1) / bytesPerMiB;
    return static_cast<unsigned>(std::min<quint64>(static_cast<quint64>(roundedMiB),
        static_cast<quint64>(std::numeric_limits<unsigned>::max())));
}

void configureRawPreviewMemoryLimit(LibRaw& processor)
{
    processor.imgdata.rawparams.max_raw_memory_mb
        = rawMemoryLimitMiB(kiriview::rawImageOpenWorkspaceByteCount);
}

std::optional<QSize> trustedOriginalSizeFromRawData(const QByteArray& data,
    kiriview::ImageDecodeWorkspaceLease workspaceLease, QString* errorString)
{
    if (!workspaceLease.isManaged()
        || workspaceLease.reservedByteCount() < kiriview::rawImageOpenWorkspaceByteCount) {
        setError(errorString, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        return std::nullopt;
    }

    LibRaw processor;
    configureRawPreviewMemoryLimit(processor);
    const int errorCode
        = processor.open_buffer(data.constData(), static_cast<std::size_t>(data.size()));
    if (errorCode != LIBRAW_SUCCESS) {
        setError(errorString,
            rawDecodeErrorString(
                kiriview::imageErrorActionText(kiriview::ImageErrorActionTextId::ReadRawImage),
                errorCode));
        return std::nullopt;
    }

    const QSize originalSize = libRawImageSize(processor);
    if (!validateTrustedOriginalSize(originalSize)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageSizeInvalid));
        return std::nullopt;
    }
    return originalSize;
}

bool thumbnailFitsOriginal(QSize thumbnailSize, QSize originalSize)
{
    return thumbnailSize.width() <= originalSize.width()
        && thumbnailSize.height() <= originalSize.height();
}

bool aspectCompatible(QSize thumbnailSize, QSize originalSize)
{
    if (!validImageSize(thumbnailSize) || !validImageSize(originalSize)) {
        return false;
    }

    const long double left
        = static_cast<long double>(thumbnailSize.width()) * originalSize.height();
    const long double right
        = static_cast<long double>(thumbnailSize.height()) * originalSize.width();
    const long double maximum = std::max(std::fabsl(left), std::fabsl(right));
    if (maximum <= 0.0L) {
        return false;
    }

    return std::fabsl(left - right) / maximum <= 0.01L;
}

bool validateRawPreviewImage(const QImage& image, QSize originalSize, QString* errorString)
{
    if (image.isNull() || !validImageSize(image.size())) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageInvalid));
        return false;
    }
    if (!validateTrustedOriginalSize(originalSize)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageSizeInvalid));
        return false;
    }
    if (!thumbnailFitsOriginal(image.size(), originalSize)) {
        setError(errorString, QStringLiteral("RAW embedded thumbnail is larger than source image"));
        return false;
    }
    if (!aspectCompatible(image.size(), originalSize)) {
        setError(errorString,
            QStringLiteral("RAW embedded thumbnail aspect ratio does not match source image"));
        return false;
    }
    const qsizetype byteCost = kiriview::imageByteCost(image);
    if (byteCost <= 0 || byteCost > rawEmbeddedThumbnailPreviewRasterByteLimit) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawFullDecodeTooLarge));
        return false;
    }

    return true;
}

bool rawProcessedDataSizeFitsQByteArray(decltype(libraw_processed_image_t::data_size) dataSize)
{
    if (dataSize == 0 || std::cmp_greater(dataSize, kiriview::rawImageOpenWorkspaceByteCount)) {
        return false;
    }

    using DataSize = decltype(dataSize);
    using UnsignedQSizeType = std::make_unsigned_t<qsizetype>;
    constexpr UnsignedQSizeType maxQByteArraySize
        = static_cast<UnsignedQSizeType>(std::numeric_limits<qsizetype>::max());
    if constexpr (std::numeric_limits<DataSize>::max() > maxQByteArraySize) {
        return dataSize <= static_cast<DataSize>(maxQByteArraySize);
    }

    return true;
}

std::optional<QImage> jpegThumbnailImage(
    const libraw_processed_image_t* processedImage, QString* errorString)
{
    if (!rawProcessedDataSizeFitsQByteArray(processedImage->data_size)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedPixelDataInvalid));
        return std::nullopt;
    }

    const QByteArray jpegData = QByteArray::fromRawData(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- LibRaw byte API.
        reinterpret_cast<const char*>(processedImage->data),
        static_cast<qsizetype>(processedImage->data_size));
    kiriview::BufferedImageReader reader(jpegData, QByteArrayLiteral("jpeg"));
    if (!reader.canRead()) {
        setError(errorString,
            reader.errorString().isEmpty()
                ? kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageInvalid)
                : reader.errorString());
        return std::nullopt;
    }

    const QSize decodedSize
        = reader.transformation().toInt() & QImageIOHandler::TransformationRotate90
        ? reader.size().transposed()
        : reader.size();
    if (!rawEmbeddedThumbnailRasterFitsAdmission(decodedSize)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageSizeInvalid));
        return std::nullopt;
    }

    QImage image = reader.read();
    if (image.isNull()) {
        setError(errorString,
            reader.errorString().isEmpty()
                ? kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageInvalid)
                : reader.errorString());
        return std::nullopt;
    }
    return kiriview::displayReadyImage(image);
}

std::optional<QImage> bitmapThumbnailImage(
    const libraw_processed_image_t* processedImage, QString* errorString)
{
    if (processedImage->bits != 8 || (processedImage->colors != 3 && processedImage->colors != 4)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedPixelFormatUnsupported));
        return std::nullopt;
    }

    const QSize imageSize(processedImage->width, processedImage->height);
    if (!validImageSize(imageSize) || !rawEmbeddedThumbnailRasterFitsAdmission(imageSize)) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageSizeInvalid));
        return std::nullopt;
    }

    const std::size_t channelCount = processedImage->colors;
    const std::size_t minimumDataSize = static_cast<std::size_t>(processedImage->width)
        * static_cast<std::size_t>(processedImage->height) * channelCount;
    if (processedImage->data_size < minimumDataSize) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedPixelDataInvalid));
        return std::nullopt;
    }

    QImage image(imageSize, QImage::Format_RGBA8888);
    if (image.isNull()) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageAllocationFailed));
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

std::optional<QImage> thumbnailImageFromProcessedRaw(
    const libraw_processed_image_t* processedImage, QString* errorString, bool* unsupported)
{
    if (unsupported != nullptr) {
        *unsupported = false;
    }
    if (processedImage == nullptr) {
        setError(errorString,
            kiriview::imageErrorText(kiriview::ImageErrorTextId::RawDecodedImageInvalid));
        return std::nullopt;
    }

    switch (processedImage->type) {
    case LIBRAW_IMAGE_JPEG:
        return jpegThumbnailImage(processedImage, errorString);
    case LIBRAW_IMAGE_BITMAP:
        return bitmapThumbnailImage(processedImage, errorString);
    case LIBRAW_IMAGE_JPEGXL:
    default:
        if (unsupported != nullptr) {
            *unsupported = true;
        }
        return std::nullopt;
    }
}
}

namespace kiriview {
qsizetype rawEmbeddedThumbnailPreviewWorkspaceByteCount()
{
    return rawEmbeddedThumbnailPreviewPeakByteCount;
}

std::optional<QSize> rawEmbeddedThumbnailPreviewTrustedOriginalSize(
    const QByteArray& data, const ImageDecodeRequest& request)
{
    ImageDecodeWorkspaceBudget workspaceBudget(
        rawImageOpenWorkspaceByteCount, rawImageOpenWorkspaceByteCount);
    ImageDecodeWorkspaceLease workspaceLease
        = ImageDecodeWorkspaceDetail::startLease(workspaceBudget);
    if (!ImageDecodeWorkspaceDetail::tryReserve(workspaceLease, rawImageOpenWorkspaceByteCount)) {
        return std::nullopt;
    }
    return admittedRawEmbeddedThumbnailPreviewTrustedOriginalSize(
        data, request, std::move(workspaceLease));
}

std::optional<QSize> admittedRawEmbeddedThumbnailPreviewTrustedOriginalSize(const QByteArray& data,
    const ImageDecodeRequest& request, ImageDecodeWorkspaceLease workspaceLease)
{
    if (!rawInputIsEligibleForEmbeddedThumbnail(data, request)) {
        return std::nullopt;
    }

    return trustedOriginalSizeFromRawData(data, std::move(workspaceLease), nullptr);
}

RawEmbeddedThumbnailPreviewResult rawEmbeddedThumbnailPreviewResult(
    const QByteArray& data, const ImageDecodeRequest& request)
{
    ImageDecodeWorkspaceBudget workspaceBudget(
        rawEmbeddedThumbnailPreviewPeakByteCount, rawEmbeddedThumbnailPreviewPeakByteCount);
    ImageDecodeWorkspaceLease workspaceLease
        = ImageDecodeWorkspaceDetail::startLease(workspaceBudget);
    if (!ImageDecodeWorkspaceDetail::tryReserve(
            workspaceLease, rawEmbeddedThumbnailPreviewPeakByteCount)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing);
    }
    return admittedRawEmbeddedThumbnailPreviewResult(data, request, std::move(workspaceLease));
}

RawEmbeddedThumbnailPreviewResult admittedRawEmbeddedThumbnailPreviewResult(const QByteArray& data,
    const ImageDecodeRequest& request, ImageDecodeWorkspaceLease workspaceLease)
{
    if (!rawInputIsEligibleForEmbeddedThumbnail(data, request)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing);
    }
    if (!workspaceLease.isManaged()
        || workspaceLease.reservedByteCount() < rawEmbeddedThumbnailPreviewPeakByteCount) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing);
    }

    QString errorString;
    LibRaw processor;
    configureRawPreviewMemoryLimit(processor);
    int errorCode = processor.open_buffer(data.constData(), static_cast<std::size_t>(data.size()));
    if (errorCode != LIBRAW_SUCCESS) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Failed, {}, {},
            rawDecodeErrorString(
                imageErrorActionText(ImageErrorActionTextId::ReadRawImage), errorCode));
    }

    const QSize originalSize = libRawImageSize(processor);
    if (!validateTrustedOriginalSize(originalSize)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Invalid, {}, originalSize,
            imageErrorText(ImageErrorTextId::RawDecodedImageSizeInvalid));
    }

    errorCode = processor.unpack_thumb();
    if (rawThumbnailMissingError(errorCode)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing, {}, originalSize);
    }
    if (errorCode != LIBRAW_SUCCESS) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Failed, {}, originalSize,
            rawDecodeErrorString(
                imageErrorActionText(ImageErrorActionTextId::UnpackRawImage), errorCode));
    }

    int memImageErrorCode = LIBRAW_SUCCESS;
    ProcessedRawImage processedImage(
        processor.dcraw_make_mem_thumb(&memImageErrorCode), &LibRaw::dcraw_clear_mem);
    if (rawThumbnailMissingError(memImageErrorCode)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing, {}, originalSize);
    }
    if (memImageErrorCode != LIBRAW_SUCCESS) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Failed, {}, originalSize,
            rawDecodeErrorString(imageErrorActionText(ImageErrorActionTextId::CreateDisplayImage),
                memImageErrorCode));
    }

    bool unsupportedThumbnail = false;
    std::optional<QImage> image
        = thumbnailImageFromProcessedRaw(processedImage.get(), &errorString, &unsupportedThumbnail);
    if (unsupportedThumbnail) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing, {}, originalSize);
    }
    if (!image.has_value()) {
        return rawResult(
            RawEmbeddedThumbnailPreviewStatus::Invalid, {}, originalSize, std::move(errorString));
    }
    if (!validateRawPreviewImage(*image, originalSize, &errorString)) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Invalid, std::move(*image),
            originalSize, std::move(errorString));
    }

    const qsizetype retainedByteCount = imageByteCost(*image);
    ImageDecodeWorkspaceHold retainedWorkspace = workspaceLease.retainOnly(retainedByteCount);
    if (!retainedWorkspace.isManaged()) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing, {}, originalSize);
    }
    *image = imageRetainingDecodeWorkspace(std::move(*image), std::move(retainedWorkspace));
    if (image->isNull()) {
        return rawResult(RawEmbeddedThumbnailPreviewStatus::Missing, {}, originalSize);
    }

    return rawResult(RawEmbeddedThumbnailPreviewStatus::Ready, std::move(*image), originalSize);
}

std::optional<StaticDisplayImagePayload> rawEmbeddedThumbnailPreviewDisplayPayload(
    const ImageDecodeRequest& request, const RawEmbeddedThumbnailPreviewResult& result)
{
    if (result.status != RawEmbeddedThumbnailPreviewStatus::Ready) {
        return std::nullopt;
    }

    QImage image = displayReadyImage(result.image);
    QString errorString;
    if (!validateRawPreviewImage(image, result.originalSize, &errorString)) {
        return std::nullopt;
    }

    return StaticDisplayImagePayload {
        sourceKeyForUrl(request.imageUrl()).identity,
        {},
        result.originalSize,
        std::move(image),
        DisplayImageQuality::ThumbnailPreview,
        {},
        {},
        {},
        nullptr,
        DisplayImagePreviewOrigin::RawEmbeddedThumbnail,
        StaticImageSourceDetailModel::FiniteRaster,
        request.sourceRevision(),
        DisplayImageRasterKind::ProvisionalPreview,
    };
}
}
