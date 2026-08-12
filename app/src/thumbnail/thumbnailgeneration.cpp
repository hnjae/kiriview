// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnailgeneration.h"

#include "archive/mediaentrysourcebackend.h"
#include "async/imageioworkerjob.h"
#include "bridge/rustqtconversion.h"
#include "cache/imagebyteaccounting.h"
#include "decoding/decodedimageresult.h"
#include "decoding/kiriimagedecoder.h"
#include "kiriview/src/support/thumbnailcache.cxx.h"
#include "localization/mediaentrysourceerrortext.h"
#include "rendering/imagerendering.h"
#include "rendering/staticimage.h"
#include "session/thumbnaillogging.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/videothumbnailextractionadapter.h"

#include <QFile>
#include <QImage>
#include <QSize>
#include <Qt>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <utility>
#include <variant>

namespace {
QString projectThumbnailMediaEntrySourceError(const kiriview::MediaEntrySourceError& error)
{
    qCWarning(kiriviewThumbnailLog).noquote() << "collection thumbnail access failed" << error;
    return kiriview::mediaEntrySourceErrorText(error);
}

kiriview::RustThumbnailCacheBucket rustBucket(
    kiriview::ActiveNavigationThumbnailDemandBucket bucket)
{
    switch (bucket) {
    case kiriview::ActiveNavigationThumbnailDemandBucket::None:
        return kiriview::RustThumbnailCacheBucket::None;
    case kiriview::ActiveNavigationThumbnailDemandBucket::Normal:
        return kiriview::RustThumbnailCacheBucket::Normal;
    case kiriview::ActiveNavigationThumbnailDemandBucket::Large:
        return kiriview::RustThumbnailCacheBucket::Large;
    case kiriview::ActiveNavigationThumbnailDemandBucket::XLarge:
        return kiriview::RustThumbnailCacheBucket::XLarge;
    case kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge:
        return kiriview::RustThumbnailCacheBucket::XxLarge;
    }

    return kiriview::RustThumbnailCacheBucket::None;
}

kiriview::ActiveNavigationThumbnailDemandBucket thumbnailBucket(
    kiriview::RustThumbnailCacheBucket bucket)
{
    switch (bucket) {
    case kiriview::RustThumbnailCacheBucket::None:
        return kiriview::ActiveNavigationThumbnailDemandBucket::None;
    case kiriview::RustThumbnailCacheBucket::Normal:
        return kiriview::ActiveNavigationThumbnailDemandBucket::Normal;
    case kiriview::RustThumbnailCacheBucket::Large:
        return kiriview::ActiveNavigationThumbnailDemandBucket::Large;
    case kiriview::RustThumbnailCacheBucket::XLarge:
        return kiriview::ActiveNavigationThumbnailDemandBucket::XLarge;
    case kiriview::RustThumbnailCacheBucket::XxLarge:
        return kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge;
    }

    return kiriview::ActiveNavigationThumbnailDemandBucket::None;
}

int bucketMaxEdge(kiriview::ActiveNavigationThumbnailDemandBucket bucket)
{
    switch (bucket) {
    case kiriview::ActiveNavigationThumbnailDemandBucket::Normal:
        return 128;
    case kiriview::ActiveNavigationThumbnailDemandBucket::Large:
        return 256;
    case kiriview::ActiveNavigationThumbnailDemandBucket::XLarge:
        return 512;
    case kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge:
        return 1024;
    case kiriview::ActiveNavigationThumbnailDemandBucket::None:
        break;
    }

    return 0;
}

kiriview::ThumbnailGenerationStatus generationStatusForVideoExtractionFailure(
    kiriview::VideoThumbnailExtractionFailureCause cause)
{
    using Cause = kiriview::VideoThumbnailExtractionFailureCause;
    using Status = kiriview::ThumbnailGenerationStatus;

    switch (cause) {
    case Cause::InvalidRequest:
        return Status::VideoExtractionInvalidRequest;
    case Cause::SourceUnavailable:
        return Status::VideoSourceUnavailable;
    case Cause::UnsupportedMedia:
        return Status::VideoUnsupportedMedia;
    case Cause::BackendFailure:
        return Status::VideoBackendFailure;
    case Cause::TimedOut:
        return Status::VideoExtractionTimedOut;
    case Cause::NoRepresentativeImage:
        return Status::VideoNoRepresentativeImage;
    case Cause::ResourceLimit:
        return Status::ResourceLimitExceeded;
    }

    return Status::Failed;
}

kiriview::ThumbnailGenerationResult failedResult(
    kiriview::ActiveNavigationThumbnailDemandBucket bucket, QString errorString,
    kiriview::ThumbnailGenerationStatus status = kiriview::ThumbnailGenerationStatus::Failed)
{
    return kiriview::ThumbnailGenerationResult {
        status,
        {},
        {},
        bucket,
        {},
        std::move(errorString),
    };
}

kiriview::ThumbnailGenerationResult readyResultFromCache(
    const kiriview::ThumbnailCacheLookupResult& lookup)
{
    return kiriview::ThumbnailGenerationResult {
        kiriview::ThumbnailGenerationStatus::Ready,
        {},
        lookup.image,
        lookup.requestedBucket,
        lookup.sourceCachePath,
        {},
    };
}

QSize boundedSize(QSize size, int maximumLongEdge)
{
    if (size.isEmpty() || maximumLongEdge <= 0) {
        return {};
    }

    const int longEdge = std::max(size.width(), size.height());
    if (longEdge <= maximumLongEdge) {
        return size;
    }

    return size.scaled(QSize(maximumLongEdge, maximumLongEdge), Qt::KeepAspectRatio);
}

QImage thumbnailFrame(QImage image, int maximumLongEdge)
{
    const QSize targetSize = boundedSize(image.size(), maximumLongEdge);
    if (targetSize.isEmpty()) {
        return {};
    }
    if (targetSize == image.size()) {
        return image;
    }

    try {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } catch (const std::bad_alloc&) {
        return {};
    }
}

QImage renderedThumbnailImage(const kiriview::DecodedImage& decoded, int maximumLongEdge,
    QString* errorString, kiriview::DecodedImageFailureCause* failureCause)
{
    return std::visit(
        [maximumLongEdge, errorString, failureCause](const auto& image) -> QImage {
            using Image = std::decay_t<decltype(image)>;
            if constexpr (std::is_same_v<Image, kiriview::StaticDecodedImage>) {
                if (image.displayImage.refinementSource == nullptr) {
                    if (errorString != nullptr) {
                        *errorString = QStringLiteral("static image source is unavailable");
                    }
                    return {};
                }
                const QSize targetSize = boundedSize(
                    image.displayImage.refinementSource->imageSize(), maximumLongEdge);
                kiriview::StaticImageDisplayDecodeResult result
                    = image.displayImage.refinementSource->decodeRasterDisplayImage(targetSize);
                if (result.image.isNull() && errorString != nullptr) {
                    *errorString = result.diagnostics.userMessage;
                }
                if (result.image.isNull() && failureCause != nullptr
                    && result.failureCause
                        == kiriview::StaticImageDisplayDecodeFailureCause::ResourceExhausted) {
                    *failureCause = kiriview::DecodedImageFailureCause::ResourceLimitExceeded;
                }
                return result.image;
            } else {
                QImage rendered = thumbnailFrame(image.firstFrame, maximumLongEdge);
                if (rendered.isNull() && failureCause != nullptr) {
                    *failureCause = kiriview::DecodedImageFailureCause::ResourceLimitExceeded;
                }
                return rendered;
            }
        },
        decoded);
}

struct ThumbnailTransformationPlan
{
    qsizetype outputByteCount = 0;
    qsizetype transientByteCount = 0;
    bool renderCreatesOutput = false;
};

std::optional<qsizetype> thumbnailSmoothScaleTransientByteCount(QSize sourceSize, QSize targetSize)
{
    if (sourceSize.isEmpty() || targetSize.isEmpty() || sourceSize == targetSize) {
        return qsizetype(0);
    }

    // Qt smooth scaling currently allocates coordinate/weight arrays and may queue one task per
    // output row. Charge deliberately broad per-row headroom so these short-lived allocations do
    // not hide behind the independently retained output reservation.
    constexpr qsizetype fixedByteCount = qsizetype { 1 } * 1024 * 1024;
    constexpr qsizetype coordinateBytesPerDimension = 16;
    constexpr qsizetype schedulingBytesPerOutputRow = qsizetype { 64 } * 1024;
    const qsizetype coordinateByteCount = kiriview::saturatedQtByteProduct(
        static_cast<qsizetype>(targetSize.width()) + targetSize.height(),
        coordinateBytesPerDimension);
    const qsizetype schedulingByteCount
        = kiriview::saturatedQtByteProduct(targetSize.height(), schedulingBytesPerOutputRow);
    const qsizetype transientByteCount = kiriview::saturatedQtByteSum(
        fixedByteCount, kiriview::saturatedQtByteSum(coordinateByteCount, schedulingByteCount));
    return transientByteCount == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(transientByteCount);
}

std::optional<ThumbnailTransformationPlan> thumbnailTransformationPlan(
    const kiriview::DecodedImage& decoded, int maximumLongEdge)
{
    return std::visit(
        [maximumLongEdge](const auto& image) -> std::optional<ThumbnailTransformationPlan> {
            using Image = std::decay_t<decltype(image)>;
            if constexpr (std::is_same_v<Image, kiriview::StaticDecodedImage>) {
                if (image.displayImage.refinementSource == nullptr) {
                    return std::nullopt;
                }
                const QSize sourceSize = image.displayImage.refinementSource->imageSize();
                const QSize targetSize = boundedSize(sourceSize, maximumLongEdge);
                const std::optional<qsizetype> outputByteCount
                    = kiriview::checkedImageDecodeWorkspaceByteCount(targetSize, 4, 1);
                const std::optional<qsizetype> producerPeakByteCount
                    = image.displayImage.refinementSource->rasterDisplayRefinementPeakByteCost(
                        targetSize);
                if (!outputByteCount.has_value() || !producerPeakByteCount.has_value()
                    || *producerPeakByteCount < *outputByteCount) {
                    return std::nullopt;
                }
                return ThumbnailTransformationPlan {
                    *outputByteCount,
                    *producerPeakByteCount - *outputByteCount,
                    true,
                };
            } else {
                const QSize targetSize = boundedSize(image.firstFrame.size(), maximumLongEdge);
                const bool createsOutput = targetSize != image.firstFrame.size();
                const std::optional<qsizetype> outputByteCount
                    = kiriview::checkedImageDecodeWorkspaceByteCount(targetSize, 4, 1);
                const std::optional<qsizetype> transientByteCount = createsOutput
                    ? thumbnailSmoothScaleTransientByteCount(image.firstFrame.size(), targetSize)
                    : std::optional<qsizetype>(0);
                if (!outputByteCount.has_value() || !transientByteCount.has_value()) {
                    return std::nullopt;
                }
                return ThumbnailTransformationPlan {
                    *outputByteCount,
                    *transientByteCount,
                    createsOutput,
                };
            }
        },
        decoded);
}

kiriview::ThumbnailGenerationImageDecodeResult renderDecodedThumbnailImageImpl(
    const kiriview::DecodedImage& decoded, int maximumLongEdge,
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& workspaceBudget)
{
    kiriview::ThumbnailGenerationWorkspaceHolds workspaceHolds;
    kiriview::ImageDecodeWorkspaceHold decodedImageWorkspaceHold = std::visit(
        [](const auto& image) -> kiriview::ImageDecodeWorkspaceHold {
            if constexpr (requires { image.firstFrameWorkspaceHold; }) {
                return image.firstFrameWorkspaceHold;
            }
            return {};
        },
        decoded);
    const qsizetype decodedImageWorkspaceByteCount = std::visit(
        [](const auto& image) -> qsizetype {
            if constexpr (requires { image.firstFrameWorkspaceHold; }) {
                return image.firstFrameWorkspaceHold.reservedByteCount();
            } else if constexpr (requires { image.displayImage.retainedRasterByteCost(); }) {
                return image.displayImage.retainedRasterByteCost();
            }
            return 0;
        },
        decoded);
    const std::optional<ThumbnailTransformationPlan> transformationPlan
        = thumbnailTransformationPlan(decoded, maximumLongEdge);
    if (!transformationPlan.has_value()) {
        return {
            {},
            {},
            {},
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
        };
    }

    kiriview::ImageDecodeWorkspaceLease transformationLease
        = workspaceBudget->startLeaseForOperation(decodedImageWorkspaceByteCount);
    const qsizetype transformationReservationByteCount = kiriview::saturatedQtByteSum(
        transformationPlan->outputByteCount, transformationPlan->transientByteCount);
    if (transformationReservationByteCount == std::numeric_limits<qsizetype>::max()
        || !transformationLease.tryReserve(transformationReservationByteCount)) {
        return {
            {},
            {},
            {},
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
        };
    }
    workspaceHolds.decodedImage = std::move(decodedImageWorkspaceHold);

    QString errorString;
    kiriview::DecodedImageFailureCause failureCause = kiriview::DecodedImageFailureCause::Unknown;
    QImage image;
    try {
        image = renderedThumbnailImage(decoded, maximumLongEdge, &errorString, &failureCause);
    } catch (const std::bad_alloc&) {
        errorString = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
        failureCause = kiriview::DecodedImageFailureCause::ResourceLimitExceeded;
    }
    if (!transformationLease.release(transformationPlan->transientByteCount)) {
        image = {};
        errorString = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
        failureCause = kiriview::DecodedImageFailureCause::ResourceLimitExceeded;
    }
    return {
        std::move(workspaceHolds),
        std::move(transformationLease),
        std::move(image),
        std::move(errorString),
        failureCause,
        transformationPlan->outputByteCount,
        transformationPlan->renderCreatesOutput,
    };
}

kiriview::ThumbnailGenerationImageDecodeResult defaultThumbnailGenerationImageDecoder(
    const QByteArray& bytes, int maximumLongEdge,
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& workspaceBudget)
{
    kiriview::DecodedImageResult decodeResult
        = kiriview::decodeImageData(bytes, kiriview::ImageDecodeRequest {}, workspaceBudget);
    if (const kiriview::DecodedImageFailure* failure
        = kiriview::decodedImageResultFailure(decodeResult)) {
        return {
            {},
            {},
            {},
            failure->errorString,
            failure->cause,
        };
    }

    kiriview::DecodedImage* decoded = kiriview::decodedImageResultImage(decodeResult);
    if (decoded == nullptr) {
        return {
            {},
            {},
            {},
            QStringLiteral("image decode produced no image"),
            kiriview::DecodedImageFailureCause::Unknown,
        };
    }

    return renderDecodedThumbnailImageImpl(*decoded, maximumLongEdge, workspaceBudget);
}

kiriview::MediaEntrySourceImageDataResult loadOpenedCollectionThumbnailBytes(
    const kiriview::ThumbnailGenerationRequest& request, kiriview::ImageSourceDataLease lease)
{
    return kiriview::loadMediaEntrySourceImageData(
        request.openedCollectionScope, request.sourceUrl, std::move(lease));
}

kiriview::MediaEntrySourceThumbnailMetadataResult loadOpenedCollectionThumbnailMetadata(
    const kiriview::ThumbnailGenerationRequest& request)
{
    return kiriview::loadMediaEntrySourceThumbnailMetadata(
        request.openedCollectionScope, request.sourceUrl);
}

std::optional<kiriview::ThumbnailOriginalIdentity> openedCollectionVirtualOriginalIdentity(
    const kiriview::MediaEntrySourceThumbnailMetadata& metadata)
{
    if (metadata.checksum.algorithm != kiriview::MediaEntryContentChecksumAlgorithm::Crc32
        || metadata.checksum.value > std::numeric_limits<std::uint32_t>::max()
        || metadata.uncompressedSize < 0) {
        return std::nullopt;
    }

    const QString uri
        = kiriview::Bridge::qtString(kiriview::rustVirtualThumbnailArchiveEntryCrc32Uri(
            static_cast<std::uint32_t>(metadata.checksum.value),
            static_cast<std::uint64_t>(metadata.uncompressedSize)));
    if (uri.isEmpty()) {
        return std::nullopt;
    }

    return kiriview::ThumbnailOriginalIdentity::fromNonFileUri(
        uri, 0, metadata.uncompressedSize, QString());
}

kiriview::ImageSourceData defaultThumbnailGenerationBytesLoader(
    const kiriview::ThumbnailGenerationRequest& request,
    const std::shared_ptr<kiriview::ImageSourceDataBudget>& sourceDataBudget, QString* errorString)
{
    kiriview::ImageSourceDataLease lease = sourceDataBudget->startLease();
    if (!request.openedCollectionScope.isEmpty()) {
        kiriview::MediaEntrySourceImageDataResult dataResult
            = loadOpenedCollectionThumbnailBytes(request, std::move(lease));
        if (const auto* error = kiriview::mediaEntrySourceResultError(dataResult)) {
            QString projectedError = projectThumbnailMediaEntrySourceError(*error);
            if (errorString != nullptr) {
                *errorString = std::move(projectedError);
            }
            return {};
        }
        if (auto* data = kiriview::mediaEntrySourceResultValue(dataResult)) {
            return kiriview::ImageSourceData(std::move(data->data), std::move(data->lease));
        }
        if (errorString != nullptr) {
            *errorString = QStringLiteral("collection thumbnail bytes are unavailable");
        }
        return {};
    }

    QFile file(QFile::decodeName(request.localPathBytes));
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString != nullptr) {
            *errorString = file.errorString();
        }
        return {};
    }

    const kiriview::ImageSourceDataReadResult readResult
        = kiriview::readImageSourceData(file, std::move(lease), file.size());
    if (readResult.status != kiriview::ImageSourceDataReadStatus::Ready) {
        if (errorString != nullptr) {
            *errorString = readResult.diagnosticDetail;
        }
        return {};
    }
    return readResult.sourceData;
}

std::optional<kiriview::ThumbnailOriginalIdentity> defaultOpenedCollectionOriginalIdentityLoader(
    const kiriview::ThumbnailGenerationRequest& request, QString* errorString)
{
    kiriview::MediaEntrySourceThumbnailMetadataResult metadataResult
        = loadOpenedCollectionThumbnailMetadata(request);
    if (const auto* error = kiriview::mediaEntrySourceResultError(metadataResult)) {
        QString projectedError = projectThumbnailMediaEntrySourceError(*error);
        if (errorString != nullptr) {
            *errorString = std::move(projectedError);
        }
        return std::nullopt;
    }

    const auto* metadata = kiriview::mediaEntrySourceResultValue(metadataResult);
    if (metadata == nullptr) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("collection thumbnail metadata failed");
        }
        return std::nullopt;
    }

    std::optional<kiriview::ThumbnailOriginalIdentity> identity
        = openedCollectionVirtualOriginalIdentity(*metadata);
    if (!identity.has_value() && errorString != nullptr) {
        *errorString = QStringLiteral("collection thumbnail identity failed");
    }
    return identity;
}

std::optional<kiriview::ThumbnailCacheLookupResult> defaultThumbnailGenerationCacheLookup(
    const kiriview::ThumbnailOriginalIdentity& identity,
    kiriview::ActiveNavigationThumbnailDemandBucket requestedBucket,
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& workspaceBudget)
{
    kiriview::ThumbnailCacheLookupRequest request;
    request.localPathBytes = identity.localPathBytes;
    request.originalIdentity = identity;
    request.requestedBucket = requestedBucket;
    return kiriview::lookupThumbnailCache(request, workspaceBudget);
}

kiriview::ThumbnailGenerationCacheInstallResult installThumbnail(
    const kiriview::ThumbnailOriginalIdentity& identity,
    kiriview::ActiveNavigationThumbnailDemandBucket requestedBucket, const QImage& rgba8)
{
    const rust::Slice<const std::uint8_t> pixels(
        rgba8.constBits(), static_cast<std::size_t>(rgba8.sizeInBytes()));
    kiriview::RustThumbnailCacheInstallResult install;
    if (identity.isNonFileUri()) {
        const QByteArray uri = identity.uri.toUtf8();
        const QByteArray mimeType = identity.mimeType.toUtf8();
        install = kiriview::rustInstallDisplayThumbnailNonFileUriRgba8(
            kiriview::Bridge::rustStr(uri), identity.mtimeSeconds, identity.originalByteSize,
            kiriview::Bridge::rustStr(mimeType), rustBucket(requestedBucket), rgba8.width(),
            rgba8.height(), rgba8.bytesPerLine(), pixels);
    } else {
        install = kiriview::rustInstallDisplayThumbnailRgba8(
            kiriview::Bridge::rustBytes(identity.localPathBytes), rustBucket(requestedBucket),
            rgba8.width(), rgba8.height(), rgba8.bytesPerLine(), pixels);
    }

    return kiriview::ThumbnailGenerationCacheInstallResult {
        install.success,
        thumbnailBucket(install.requested_bucket),
        kiriview::Bridge::qtString(install.installed_cache_path),
        kiriview::Bridge::qtString(install.error),
    };
}

kiriview::ThumbnailGenerationResult finishGeneratedThumbnailImage(
    const kiriview::ThumbnailGenerationRequest& request,
    const kiriview::ThumbnailOriginalIdentity& originalIdentity,
    kiriview::ThumbnailGenerationWorkspaceHolds workspaceHolds,
    kiriview::ImageDecodeWorkspaceLease transformationLease, QImage image,
    qsizetype transformationOutputByteCount, bool imageUsesTransformationReservation,
    const kiriview::ThumbnailGenerationDependencies& dependencies)
{
    if (transformationOutputByteCount <= 0) {
        const std::optional<qsizetype> measuredOutputByteCount
            = kiriview::checkedImageDecodeWorkspaceByteCount(image.size(), 4, 1);
        if (!measuredOutputByteCount.has_value()) {
            image = {};
            workspaceHolds = {};
            return failedResult(request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
        }
        transformationOutputByteCount = *measuredOutputByteCount;
    }
    if (!transformationLease.isManaged()) {
        transformationLease = dependencies.workspaceBudget->startLeaseForOperation(
            workspaceHolds.decodedImage.reservedByteCount());
        if (!transformationLease.tryReserve(transformationOutputByteCount)) {
            image = {};
            workspaceHolds = {};
            return failedResult(request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
        }
        imageUsesTransformationReservation = false;
    }

    const bool conversionCreatesOutput = image.format() != QImage::Format_RGBA8888;
    if (conversionCreatesOutput && imageUsesTransformationReservation
        && !transformationLease.tryReserve(transformationOutputByteCount)) {
        image = {};
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }
    QImage rgba8;
    try {
        rgba8 = image.convertToFormat(QImage::Format_RGBA8888);
    } catch (const std::bad_alloc&) {
        image = {};
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }
    if (rgba8.isNull()) {
        image = {};
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }

    image = {};
    workspaceHolds.decodedImage = {};
    kiriview::ImageDecodeWorkspaceHold retainedOutputWorkspace
        = transformationLease.retainOnly(transformationOutputByteCount);
    if (!retainedOutputWorkspace.isManaged()) {
        rgba8 = {};
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }
    QImage admittedImage = kiriview::imageRetainingDecodeWorkspace(
        std::move(rgba8), std::move(retainedOutputWorkspace));
    if (admittedImage.isNull()) {
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }
    workspaceHolds = {};
    rgba8 = std::move(admittedImage);

    if (!request.cacheInstallEnabled) {
        return kiriview::ThumbnailGenerationResult {
            kiriview::ThumbnailGenerationStatus::Ready,
            std::move(workspaceHolds),
            std::move(rgba8),
            request.requestedBucket,
            {},
            {},
        };
    }

    const kiriview::ThumbnailGenerationCacheInstallResult install
        = dependencies.cacheRepository.install(originalIdentity, request.requestedBucket, rgba8);
    if (!install.success) {
        return kiriview::ThumbnailGenerationResult {
            kiriview::ThumbnailGenerationStatus::Ready,
            std::move(workspaceHolds),
            std::move(rgba8),
            install.requestedBucket,
            {},
            install.errorString,
            kiriview::ThumbnailGenerationDiagnosticKind::CacheInstallFailed,
        };
    }

    return kiriview::ThumbnailGenerationResult {
        kiriview::ThumbnailGenerationStatus::Ready,
        std::move(workspaceHolds),
        std::move(rgba8),
        install.requestedBucket,
        install.installedCachePath,
        {},
    };
}

kiriview::ThumbnailGenerationDependencies resolvedThumbnailGenerationDependencies(
    kiriview::ThumbnailGenerationDependencies dependencies)
{
    if (dependencies.sourceDataBudget == nullptr) {
        dependencies.sourceDataBudget = kiriview::defaultImageSourceDataBudget();
    }
    if (dependencies.workspaceBudget == nullptr) {
        dependencies.workspaceBudget = kiriview::defaultImageDecodeWorkspaceBudget();
    }
    if (!dependencies.bytesLoader) {
        dependencies.bytesLoader = [sourceDataBudget = dependencies.sourceDataBudget](
                                       const kiriview::ThumbnailGenerationRequest& request,
                                       QString* errorString) {
            return defaultThumbnailGenerationBytesLoader(request, sourceDataBudget, errorString);
        };
    }
    if (!dependencies.imageDecoder) {
        dependencies.imageDecoder = [workspaceBudget = dependencies.workspaceBudget](
                                        const QByteArray& bytes, int maximumLongEdge) {
            return defaultThumbnailGenerationImageDecoder(bytes, maximumLongEdge, workspaceBudget);
        };
    }
    if (!dependencies.maximumLongEdgeForBucket) {
        dependencies.maximumLongEdgeForBucket = bucketMaxEdge;
    }
    if (!dependencies.openedCollectionOriginalIdentityLoader) {
        dependencies.openedCollectionOriginalIdentityLoader
            = defaultOpenedCollectionOriginalIdentityLoader;
    }
    if (!dependencies.cacheRepository.lookup) {
        dependencies.cacheRepository.lookup
            = [workspaceBudget = dependencies.workspaceBudget](
                  const kiriview::ThumbnailOriginalIdentity& identity,
                  kiriview::ActiveNavigationThumbnailDemandBucket requestedBucket) {
                  return defaultThumbnailGenerationCacheLookup(
                      identity, requestedBucket, workspaceBudget);
              };
    }
    if (!dependencies.cacheRepository.install) {
        dependencies.cacheRepository.install = installThumbnail;
    }
    if (!dependencies.videoExtractionProvider) {
        dependencies.videoExtractionProvider = kiriview::startThumbnailVideoExtractionJob;
    }
    return dependencies;
}

kiriview::ThumbnailGenerationResult generateThumbnailWithDependencies(
    const kiriview::ThumbnailGenerationRequest& request,
    const kiriview::ThumbnailGenerationDependencies& dependencies)
{
    const int maximumLongEdge = dependencies.maximumLongEdgeForBucket(request.requestedBucket);
    if (maximumLongEdge <= 0) {
        return failedResult(
            request.requestedBucket, QStringLiteral("thumbnail generation requires a size bucket"));
    }

    kiriview::ThumbnailOriginalIdentity originalIdentity = request.originalIdentity.isValid()
        ? request.originalIdentity
        : kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(request.localPathBytes);
    if (!request.openedCollectionScope.isEmpty()) {
        QString identityError;
        const std::optional<kiriview::ThumbnailOriginalIdentity> virtualIdentity
            = dependencies.openedCollectionOriginalIdentityLoader(request, &identityError);
        if (!virtualIdentity.has_value()) {
            return failedResult(request.requestedBucket,
                identityError.isEmpty() ? QStringLiteral("collection thumbnail identity failed")
                                        : std::move(identityError));
        }
        originalIdentity = *virtualIdentity;
        if (request.cacheInstallEnabled) {
            const std::optional<kiriview::ThumbnailCacheLookupResult> lookup
                = dependencies.cacheRepository.lookup(originalIdentity, request.requestedBucket);
            if (lookup.has_value()
                && lookup->status == kiriview::ThumbnailCacheLookupStatus::Ready) {
                return readyResultFromCache(*lookup);
            }
            if (lookup.has_value()
                && (lookup->status == kiriview::ThumbnailCacheLookupStatus::Invalid
                    || lookup->status == kiriview::ThumbnailCacheLookupStatus::Failed)) {
                return failedResult(request.requestedBucket, lookup->errorString);
            }
            if (lookup.has_value()
                && lookup->status == kiriview::ThumbnailCacheLookupStatus::ResourceLimitExceeded) {
                return failedResult(request.requestedBucket, lookup->errorString,
                    kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
            }
        }
    }

    QString loadError;
    kiriview::ImageSourceData sourceData = dependencies.bytesLoader(request, &loadError);
    if (sourceData.data.isEmpty() && !loadError.isEmpty()) {
        return failedResult(request.requestedBucket, std::move(loadError));
    }
    if (!sourceData.lease.isManaged()) {
        sourceData.lease = dependencies.sourceDataBudget->startLease();
    }
    const qsizetype reservedByteCount = sourceData.lease.reservedByteCount();
    if (sourceData.data.size() > reservedByteCount
        && !sourceData.lease.tryReserve(sourceData.data.size() - reservedByteCount)) {
        return failedResult(
            request.requestedBucket, kiriview::imageSourceDataResourceLimitDiagnostic());
    }

    kiriview::ThumbnailGenerationImageDecodeResult decodeResult
        = dependencies.imageDecoder(std::move(sourceData.data), maximumLongEdge);
    if (decodeResult.image.isNull()) {
        const kiriview::ThumbnailGenerationStatus status
            = decodeResult.failureCause == kiriview::DecodedImageFailureCause::ResourceLimitExceeded
            ? kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded
            : kiriview::ThumbnailGenerationStatus::Failed;
        return failedResult(request.requestedBucket,
            decodeResult.errorString.isEmpty()
                ? QStringLiteral("thumbnail render produced no image")
                : std::move(decodeResult.errorString),
            status);
    }

    return finishGeneratedThumbnailImage(request, originalIdentity,
        std::move(decodeResult.workspaceHolds), std::move(decodeResult.transformationLease),
        std::move(decodeResult.image), decodeResult.transformationOutputByteCount,
        decodeResult.imageUsesTransformationReservation, dependencies);
}

kiriview::ImageIoJob startVideoThumbnailGenerationJob(QObject* receiver,
    kiriview::ThumbnailGenerationRequest request, kiriview::ThumbnailGenerationCallback callback,
    kiriview::ThumbnailGenerationDependencies dependencies)
{
    const int maximumLongEdge = dependencies.maximumLongEdgeForBucket(request.requestedBucket);
    if (maximumLongEdge <= 0) {
        if (callback) {
            callback(failedResult(request.requestedBucket,
                QStringLiteral("thumbnail generation requires a size bucket")));
        }
        return {};
    }

    kiriview::ThumbnailOriginalIdentity originalIdentity = request.originalIdentity.isValid()
        ? request.originalIdentity
        : kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(request.localPathBytes);
    if (!originalIdentity.isValid()) {
        if (callback) {
            callback(failedResult(
                request.requestedBucket, QStringLiteral("video thumbnail identity failed")));
        }
        return {};
    }

    kiriview::VideoThumbnailExtractionRequest extractionRequest;
    extractionRequest.sourceUrl = request.sourceUrl;
    extractionRequest.maximumLongEdge = maximumLongEdge;

    std::shared_ptr<kiriview::ImageDecodeWorkspaceLease> workingLease;
    try {
        workingLease = std::make_shared<kiriview::ImageDecodeWorkspaceLease>(
            dependencies.workspaceBudget->startLease());
    } catch (const std::bad_alloc&) {
        if (callback) {
            callback(failedResult(request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
        }
        return {};
    }
    if (!workingLease->tryReserve(kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes)) {
        if (callback) {
            callback(failedResult(request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
        }
        return {};
    }

    kiriview::ThumbnailVideoExtractionProvider extractionProvider
        = std::move(dependencies.videoExtractionProvider);
    return extractionProvider(receiver, std::move(extractionRequest),
        [request = std::move(request), originalIdentity = std::move(originalIdentity),
            callback = std::move(callback), dependencies = std::move(dependencies),
            workingLease = std::move(workingLease)](
            kiriview::VideoThumbnailExtractionResult extractionResult) mutable {
            if (!callback) {
                return;
            }
            if (extractionResult.status == kiriview::VideoThumbnailExtractionStatus::Failed) {
                QString diagnostic;
                kiriview::ThumbnailGenerationStatus failureStatus
                    = kiriview::ThumbnailGenerationStatus::Failed;
                if (extractionResult.failure.has_value()) {
                    failureStatus = generationStatusForVideoExtractionFailure(
                        extractionResult.failure->cause);
                    diagnostic = std::move(extractionResult.failure->diagnostic);
                }
                callback(failedResult(request.requestedBucket,
                    diagnostic.isEmpty() ? QStringLiteral("video thumbnail extraction failed")
                                         : std::move(diagnostic),
                    failureStatus));
                return;
            }
            if (extractionResult.image.isNull() || extractionResult.failure.has_value()) {
                callback(failedResult(
                    request.requestedBucket, QStringLiteral("video thumbnail produced no image")));
                return;
            }

            const qsizetype outputByteCount = extractionResult.image.sizeInBytes();
            kiriview::ImageDecodeWorkspaceLease admittedWorkingLease = std::move(*workingLease);
            const qsizetype workingByteCount = admittedWorkingLease.reservedByteCount();
            if (outputByteCount <= 0 || outputByteCount > workingByteCount
                || !admittedWorkingLease.release(workingByteCount - outputByteCount)) {
                callback(failedResult(request.requestedBucket,
                    kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                    kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
                return;
            }
            callback(finishGeneratedThumbnailImage(request, originalIdentity, {},
                std::move(admittedWorkingLease), std::move(extractionResult.image), outputByteCount,
                true, dependencies));
        });
}
}

namespace kiriview {
ThumbnailGenerationImageDecodeResult renderDecodedThumbnailImage(const DecodedImage& decoded,
    int maximumLongEdge, const std::shared_ptr<ImageDecodeWorkspaceBudget>& workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        return {
            {},
            {},
            {},
            imageDecodeWorkspaceResourceLimitDiagnostic(),
            DecodedImageFailureCause::ResourceLimitExceeded,
        };
    }
    return renderDecodedThumbnailImageImpl(decoded, maximumLongEdge, workspaceBudget);
}

ThumbnailGenerationResult generateThumbnail(
    const ThumbnailGenerationRequest& request, ThumbnailGenerationDependencies dependencies)
{
    return generateThumbnailWithDependencies(
        request, resolvedThumbnailGenerationDependencies(std::move(dependencies)));
}

ThumbnailGenerationProvider defaultThumbnailGenerationProvider(
    ImageWorkerScheduler workerScheduler, ThumbnailGenerationDependencies dependencies)
{
    return [workerScheduler = std::move(workerScheduler), dependencies = std::move(dependencies)](
               QObject* receiver, ThumbnailGenerationRequest request,
               ThumbnailGenerationCallback callback) {
        ThumbnailGenerationDependencies resolvedDependencies
            = resolvedThumbnailGenerationDependencies(dependencies);
        if (request.sourceKind == ThumbnailSourceKind::DirectVideo) {
            return startVideoThumbnailGenerationJob(
                receiver, std::move(request), std::move(callback), std::move(resolvedDependencies));
        }

        return startImageIoWorkerJob(
            receiver, workerScheduler,
            [request = std::move(request), dependencies = std::move(resolvedDependencies)]() {
                return generateThumbnail(request, dependencies);
            },
            [callback = std::move(callback)](ThumbnailGenerationResult result) mutable {
                if (callback) {
                    callback(std::move(result));
                }
            });
    };
}
}
