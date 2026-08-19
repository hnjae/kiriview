// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rawdecoder.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/imagerendering.h"
#include "decoding/staticimagedisplaysourcehelpers_p.h"
#include "imagedecodeworkspace.h"
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

void setRawDecodeFailure(QString* errorString, QString* diagnosticDetail, const QString& detail)
{
    setRawDecodeError(errorString, detail);
    setRawDecodeError(diagnosticDetail, detail);
}

QString rawBackendMessage(int errorCode)
{
    QString message = QStringLiteral("unknown LibRaw error");
    if (const char* rawMessage = LibRaw::strerror(errorCode); rawMessage != nullptr) {
        message = QString::fromUtf8(rawMessage);
    }
    return message;
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

bool validateRawImageSize(
    QSize size, QString* errorString, QString* diagnosticDetail, bool* resourceExhausted = nullptr)
{
    if (size.isEmpty()) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            QStringLiteral("RAW image size validation failed: decoded size is invalid"));
        return false;
    }

    if (kiriview::estimatedRgbaByteCost(size) > kiriview::imageFullDecodeFallbackByteLimit) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setRawDecodeFailure(errorString, diagnosticDetail,
            QStringLiteral("RAW full decode exceeds the raster byte limit"));
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

std::optional<qsizetype> rawDecoderByteLimit(QSize imageSize)
{
    const qsizetype fullRasterByteCost = kiriview::estimatedRgbaByteCost(imageSize);
    if (fullRasterByteCost <= 0) {
        return std::nullopt;
    }

    // Opening is bounded by a fixed cap before the dimensions are trusted. The same live LibRaw
    // state then grows only within this size-derived cap during unpack and processing.
    const qsizetype sizeDerivedLimit = kiriview::saturatedQtByteProduct(fullRasterByteCost, 2);
    if (sizeDerivedLimit == std::numeric_limits<qsizetype>::max()) {
        return std::nullopt;
    }
    return std::clamp(sizeDerivedLimit, kiriview::rawImageOpenWorkspaceByteCount,
        kiriview::imageFullDecodeFallbackByteLimit);
}

std::optional<qsizetype> rawProductionAdditionalPeakByteCost(QSize imageSize)
{
    const std::optional<qsizetype> decoderByteLimit = rawDecoderByteLimit(imageSize);
    const std::optional<qsizetype> rasterPeakByteCost = rawInitialRasterPeakByteCost(imageSize);
    if (!decoderByteLimit.has_value() || !rasterPeakByteCost.has_value()
        || *decoderByteLimit < kiriview::rawImageOpenWorkspaceByteCount) {
        return std::nullopt;
    }

    const qsizetype decoderGrowth = *decoderByteLimit - kiriview::rawImageOpenWorkspaceByteCount;
    const qsizetype peakByteCost = kiriview::saturatedQtByteSum(decoderGrowth, *rasterPeakByteCost);
    return peakByteCost == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(peakByteCost);
}

unsigned rawDecoderMemoryLimitMiB(qsizetype byteLimit)
{
    constexpr qsizetype bytesPerMiB = qsizetype { 1024 } * 1024;
    const qsizetype roundedMiB = (byteLimit + bytesPerMiB - 1) / bytesPerMiB;
    return static_cast<unsigned>(std::min<quint64>(static_cast<quint64>(roundedMiB),
        static_cast<quint64>(std::numeric_limits<unsigned>::max())));
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
    setRawDecodeFailure(errorString, diagnosticDetail,
        rawDecodeDiagnosticDetail(QStringLiteral("decoded-memory admission"),
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()));
}

std::optional<QImage> qImageFromRawProcessedImage(const libraw_processed_image_t* processedImage,
    QString* errorString, QString* diagnosticDetail, bool* resourceExhausted)
{
    if (processedImage == nullptr) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            QStringLiteral("RAW processed image validation failed: image is missing"));
        return std::nullopt;
    }

    if (processedImage->type != LIBRAW_IMAGE_BITMAP || processedImage->bits != 8
        || (processedImage->colors != 3 && processedImage->colors != 4)) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            QStringLiteral("RAW processed pixel format is unsupported"));
        return std::nullopt;
    }

    const QSize imageSize(processedImage->width, processedImage->height);
    if (!validateRawImageSize(imageSize, errorString, diagnosticDetail, resourceExhausted)) {
        return std::nullopt;
    }

    const std::size_t channelCount = processedImage->colors;
    const std::size_t minimumDataSize = static_cast<std::size_t>(processedImage->width)
        * static_cast<std::size_t>(processedImage->height) * channelCount;
    if (processedImage->data_size < minimumDataSize) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            QStringLiteral("RAW processed pixel data is smaller than its declared dimensions"));
        return std::nullopt;
    }

    QImage image(imageSize, QImage::Format_RGBA8888);
    if (image.isNull()) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setRawDecodeFailure(
            errorString, diagnosticDetail, QStringLiteral("RAW display-image allocation failed"));
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
    bool resourceExhausted = false;
};

RawImageProduction produceRawImage(
    LibRaw& processor, QString* errorString, QString* diagnosticDetail)
{
    RawImageProduction result;

    int errorCode = processor.unpack();
    if (errorCode != LIBRAW_SUCCESS) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            rawDecodeDiagnosticDetail(QStringLiteral("image unpack"), errorCode));
        result.resourceExhausted = errorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    errorCode = processor.dcraw_process();
    if (errorCode != LIBRAW_SUCCESS) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            rawDecodeDiagnosticDetail(QStringLiteral("image processing"), errorCode));
        result.resourceExhausted = errorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    int memImageErrorCode = LIBRAW_SUCCESS;
    ProcessedRawImage processedImage(
        processor.dcraw_make_mem_image(&memImageErrorCode), &LibRaw::dcraw_clear_mem);
    if (memImageErrorCode != LIBRAW_SUCCESS) {
        setRawDecodeFailure(errorString, diagnosticDetail,
            rawDecodeDiagnosticDetail(QStringLiteral("display-image creation"), memImageErrorCode));
        result.resourceExhausted = memImageErrorCode == LIBRAW_UNSUFFICIENT_MEMORY;
        return result;
    }

    result.image = qImageFromRawProcessedImage(
        processedImage.get(), errorString, diagnosticDetail, &result.resourceExhausted);
    return result;
}

kiriview::DecodedImageFailure rawDecodedImageFailure(
    QString diagnosticDetail, kiriview::DecodedImageFailureCause cause)
{
    return kiriview::DecodedImageFailure {
        kiriview::DecodedImageFailureRoute::Raw,
        kiriview::DecodedImageFailureOperation::DecodeRawImage,
        std::move(diagnosticDetail),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        cause,
    };
}

kiriview::DecodedImageFailure rawWorkspaceFailure()
{
    QString errorString;
    QString diagnosticDetail;
    setRawWorkspaceFailure(&errorString, &diagnosticDetail);
    return rawDecodedImageFailure(
        std::move(diagnosticDetail), kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
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
class OpenedRawImage::Private final
{
public:
    Private(ImageDecodeWorkspaceHold openWorkspaceHold, std::unique_ptr<LibRaw> processor,
        qsizetype decoderByteLimit, qsizetype productionAdditionalPeakByteCount)
        : openWorkspaceHold(std::move(openWorkspaceHold))
        , processor(std::move(processor))
        , decoderByteLimit(decoderByteLimit)
        , productionAdditionalPeakByteCount(productionAdditionalPeakByteCount)
    {
    }

    ImageDecodeWorkspaceHold openWorkspaceHold;
    std::unique_ptr<LibRaw> processor;
    qsizetype decoderByteLimit = 0;
    qsizetype productionAdditionalPeakByteCount = 0;
};

OpenedRawImage::OpenedRawImage(std::unique_ptr<Private> state)
    : d(std::move(state))
{
}

OpenedRawImage::~OpenedRawImage() = default;

OpenedRawImage::OpenedRawImage(OpenedRawImage&& other) noexcept = default;

OpenedRawImage& OpenedRawImage::operator=(OpenedRawImage&& other) noexcept = default;

qsizetype OpenedRawImage::retainedWorkspaceByteCount() const
{
    return d == nullptr ? 0 : d->openWorkspaceHold.reservedByteCount();
}

qsizetype OpenedRawImage::productionAdditionalPeakByteCount() const
{
    return d == nullptr ? 0 : d->productionAdditionalPeakByteCount;
}

DecodedImageResult OpenedRawImage::decode(
    const ImageDecodeRequest& request, ImageDecodeWorkspaceLease producerLease) &&
{
    if (d == nullptr || d->processor == nullptr || !producerLease.isManaged()
        || producerLease.reservedByteCount() < d->productionAdditionalPeakByteCount) {
        return failedDecodedImageResult(rawWorkspaceFailure());
    }

    d->processor->imgdata.params.use_camera_wb = 1;
    d->processor->imgdata.params.output_color = 1;
    d->processor->imgdata.params.output_bps = 8;
    d->processor->imgdata.rawparams.max_raw_memory_mb
        = rawDecoderMemoryLimitMiB(d->decoderByteLimit);

    QString errorString;
    QString diagnosticDetail;
    RawImageProduction production;
    try {
        production = produceRawImage(*d->processor, &errorString, &diagnosticDetail);
    } catch (const std::bad_alloc&) {
        setRawWorkspaceFailure(&errorString, &diagnosticDetail);
        production.resourceExhausted = true;
    }
    d.reset();
    if (!production.image.has_value()) {
        return failedDecodedImageResult(rawDecodedImageFailure(std::move(diagnosticDetail),
            production.resourceExhausted ? DecodedImageFailureCause::ResourceLimitExceeded
                                         : DecodedImageFailureCause::Unknown));
    }

    std::shared_ptr<StaticImageDisplaySource> source
        = std::make_shared<RawStaticImageDisplaySource>(std::move(*production.image));
    ImageDecodeWorkspaceHold retainedSourceWorkspace
        = producerLease.splitRetained(source->retainedRasterByteCost());
    if (!retainedSourceWorkspace.isManaged()) {
        return failedDecodedImageResult(DecodedImageFailure {
            DecodedImageFailureRoute::Raw,
            DecodedImageFailureOperation::DecodeRawImage,
            QStringLiteral("RAW retained raster admission split failed"),
            DecodedImageFailureSeverity::Error,
            false,
            DecodedImageFailureCause::ResourceLimitExceeded,
        });
    }
    source->retainRasterOutputWorkspace(std::move(retainedSourceWorkspace));
    return staticDecodedImageResult(std::move(source), request, &errorString, {},
        std::move(producerLease), DecodedImageFailureRoute::Raw);
}

OpenedRawImageResult openRawImageData(
    const QByteArray& data, ImageDecodeWorkspaceLease openWorkspaceLease)
{
    if (!openWorkspaceLease.isManaged()
        || openWorkspaceLease.reservedByteCount() < rawImageOpenWorkspaceByteCount) {
        return std::unexpected(rawWorkspaceFailure());
    }

    auto processor = std::make_unique<LibRaw>();
    processor->imgdata.rawparams.max_raw_memory_mb
        = rawDecoderMemoryLimitMiB(rawImageOpenWorkspaceByteCount);
    const int errorCode
        = processor->open_buffer(data.constData(), static_cast<std::size_t>(data.size()));
    if (errorCode != LIBRAW_SUCCESS) {
        return std::unexpected(rawDecodedImageFailure(
            rawDecodeDiagnosticDetail(QStringLiteral("source read"), errorCode),
            errorCode == LIBRAW_UNSUFFICIENT_MEMORY
                ? DecodedImageFailureCause::ResourceLimitExceeded
                : DecodedImageFailureCause::Unknown));
    }

    QString errorString;
    QString diagnosticDetail;
    const QSize imageSize = libRawImageSize(*processor);
    bool resourceExhausted = false;
    if (!validateRawImageSize(imageSize, &errorString, &diagnosticDetail, &resourceExhausted)) {
        return std::unexpected(rawDecodedImageFailure(std::move(diagnosticDetail),
            resourceExhausted ? DecodedImageFailureCause::ResourceLimitExceeded
                              : DecodedImageFailureCause::Unknown));
    }
    const std::optional<qsizetype> decoderByteLimit = rawDecoderByteLimit(imageSize);
    const std::optional<qsizetype> additionalPeakByteCount
        = rawProductionAdditionalPeakByteCost(imageSize);
    if (!decoderByteLimit.has_value() || !additionalPeakByteCount.has_value()) {
        return std::unexpected(rawWorkspaceFailure());
    }

    ImageDecodeWorkspaceHold openWorkspaceHold
        = openWorkspaceLease.retainOnly(rawImageOpenWorkspaceByteCount);
    if (!openWorkspaceHold.isManaged()) {
        return std::unexpected(rawWorkspaceFailure());
    }
    return std::unique_ptr<OpenedRawImage>(
        new OpenedRawImage(std::make_unique<OpenedRawImage::Private>(std::move(openWorkspaceHold),
            std::move(processor), *decoderByteLimit, *additionalPeakByteCount)));
}

DecodedImageResult decodeRawImageData(const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }

    ImageDecodeWorkspaceLease openWorkspaceLease
        = ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    if (!ImageDecodeWorkspaceDetail::tryReserve(
            openWorkspaceLease, rawImageOpenWorkspaceByteCount)) {
        return failedDecodedImageResult(rawWorkspaceFailure());
    }

    OpenedRawImageResult opened = std::unexpected(rawWorkspaceFailure());
    try {
        opened = openRawImageData(data, std::move(openWorkspaceLease));
    } catch (const std::bad_alloc&) {
        return failedDecodedImageResult(rawWorkspaceFailure());
    }
    if (!opened.has_value()) {
        return failedDecodedImageResult(std::move(opened.error()));
    }

    const qsizetype retainedWorkspaceByteCount = (*opened)->retainedWorkspaceByteCount();
    const qsizetype additionalPeakByteCount = (*opened)->productionAdditionalPeakByteCount();
    ImageDecodeWorkspaceLease producerLease = ImageDecodeWorkspaceDetail::startLeaseForOperation(
        *workspaceBudget, retainedWorkspaceByteCount);
    if (!ImageDecodeWorkspaceDetail::tryReserve(producerLease, additionalPeakByteCount)) {
        return failedDecodedImageResult(rawWorkspaceFailure());
    }
    return std::move(**opened).decode(request, std::move(producerLease));
}
}
