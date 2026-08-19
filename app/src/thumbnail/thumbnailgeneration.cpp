// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnailgeneration.h"

#include "archive/mediaentrysourcebackend.h"
#include "bridge/rustqtconversion.h"
#include "cache/imagebyteaccounting.h"
#include "decoding/decodedimageresult.h"
#include "decoding/imagedecodedependencies.h"
#include "decoding/imagedecodejob.h"
#include "decoding/imagerendering.h"
#include "decoding/kiriimagedecoder.h"
#include "decoding/staticimage.h"
#include "kiriview/src/support/thumbnailcache.cxx.h"
#include "localization/mediaentrysourceerrortext.h"
#include "session/thumbnaillogging.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/videothumbnailextractionadapter.h"

#include <QFile>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <Qt>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

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
                    *errorString = result.diagnostics.diagnosticDetail;
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

qsizetype decodedThumbnailWorkspaceByteCount(const kiriview::DecodedImage& decoded)
{
    return std::visit(
        [](const auto& image) -> qsizetype {
            if constexpr (requires { image.firstFrameWorkspaceHold; }) {
                return kiriview::saturatedQtByteSum(image.inputWorkspaceHold.reservedByteCount(),
                    image.firstFrameWorkspaceHold.reservedByteCount());
            } else if constexpr (requires { image.displayImage.retainedRasterByteCost(); }) {
                return image.displayImage.retainedRasterByteCost();
            }
            return 0;
        },
        decoded);
}

std::optional<qsizetype> thumbnailTransformationAdditionalPeakByteCount(
    const ThumbnailTransformationPlan& plan)
{
    qsizetype peakByteCount
        = kiriview::saturatedQtByteSum(plan.outputByteCount, plan.transientByteCount);
    if (plan.renderCreatesOutput) {
        peakByteCount = kiriview::saturatedQtByteSum(peakByteCount, plan.outputByteCount);
    }
    return peakByteCount == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(peakByteCount);
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
    const qsizetype decodedImageWorkspaceByteCount = decodedThumbnailWorkspaceByteCount(decoded);
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
        = kiriview::ImageDecodeWorkspaceDetail::startLeaseForOperation(
            *workspaceBudget, decodedImageWorkspaceByteCount);
    const qsizetype transformationReservationByteCount = kiriview::saturatedQtByteSum(
        transformationPlan->outputByteCount, transformationPlan->transientByteCount);
    if (transformationReservationByteCount == std::numeric_limits<qsizetype>::max()
        || !kiriview::ImageDecodeWorkspaceDetail::tryReserve(
            transformationLease, transformationReservationByteCount)) {
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
            failure->diagnosticDetail,
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
        transformationLease = kiriview::ImageDecodeWorkspaceDetail::startLeaseForOperation(
            *dependencies.workspaceBudget, workspaceHolds.decodedImage.reservedByteCount());
        if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(
                transformationLease, transformationOutputByteCount)) {
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
        && !kiriview::ImageDecodeWorkspaceDetail::tryReserve(
            transformationLease, transformationOutputByteCount)) {
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
    kiriview::ThumbnailGenerationDependencies dependencies,
    kiriview::ImageWorkerScheduler workerScheduler = {})
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
    if (!dependencies.imagePlanner) {
        dependencies.imagePlanner
            = kiriview::defaultImageDataDecodePlanner(dependencies.workspaceBudget);
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
    if (!dependencies.cacheLookupProvider) {
        dependencies.cacheLookupProvider = kiriview::defaultThumbnailCacheLookupProvider(
            std::move(workerScheduler), dependencies.workspaceBudget);
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

struct ThumbnailIdentityPreparationResult
{
    std::optional<kiriview::ThumbnailOriginalIdentity> identity;
    QString errorString;
};

struct ThumbnailSourcePreparationResult
{
    kiriview::ImageSourceData sourceData;
    QString errorString;
};

class ThumbnailGenerationAdmissionJobState final
    : public std::enable_shared_from_this<ThumbnailGenerationAdmissionJobState>
{
public:
    ThumbnailGenerationAdmissionJobState(QObject* token,
        kiriview::ImageWorkerScheduler workerScheduler,
        kiriview::ThumbnailGenerationRequest request,
        kiriview::ThumbnailGenerationDependencies dependencies,
        kiriview::ThumbnailGenerationCallback callback, bool useLegacyDecoder)
        : m_token(token)
        , m_workerScheduler(std::move(workerScheduler))
        , m_request(std::move(request))
        , m_dependencies(std::move(dependencies))
        , m_callback(std::move(callback))
        , m_useLegacyDecoder(useLegacyDecoder)
    {
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        const std::scoped_lock lock(m_mutex);
        m_completion = std::move(completion);
    }

    void start()
    {
        {
            const std::scoped_lock lock(m_mutex);
            m_self = shared_from_this();
        }
        m_maximumLongEdge = m_dependencies.maximumLongEdgeForBucket(m_request.requestedBucket);
        if (m_maximumLongEdge <= 0) {
            publish(failedResult(m_request.requestedBucket,
                QStringLiteral("thumbnail generation requires a size bucket")));
            return;
        }
        if (m_useLegacyDecoder) {
            startLegacyAdmission();
            return;
        }

        if (m_request.openedCollectionScope.isEmpty()) {
            m_originalIdentity = m_request.originalIdentity.isValid()
                ? m_request.originalIdentity
                : kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(m_request.localPathBytes);
            startSourceLoad();
            return;
        }
        startIdentityPreparation();
    }

    void cancel()
    {
        kiriview::ImageDecodeWorkspaceAdmission admission;
        std::vector<kiriview::ImageWorkerTask> workers;
        std::vector<kiriview::ImageIoJob> ioJobs;
        QPointer<kiriview::ImageDecodeJob> decodeJob;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                return;
            }
            m_logicalFinished = true;
            m_callback = {};
            admission = std::move(m_admission);
            workers = std::move(m_workers);
            ioJobs = std::move(m_ioJobs);
            decodeJob = m_decodeJob;
        }

        admission.cancel();
        for (kiriview::ImageWorkerTask& worker : workers) {
            worker.cancel();
        }
        for (kiriview::ImageIoJob& ioJob : ioJobs) {
            ioJob.cancel();
        }
        if (decodeJob != nullptr) {
            decodeJob->cancel();
        }
        maybeRetire();
    }

private:
    [[nodiscard]] bool beginChild()
    {
        const std::scoped_lock lock(m_mutex);
        if (m_logicalFinished || m_token == nullptr) {
            return false;
        }
        ++m_childCount;
        return true;
    }

    void retireChild()
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (m_childCount > 0) {
                --m_childCount;
            }
        }
        maybeRetire();
    }

    void maybeRetire()
    {
        kiriview::ImageIoJobCompletion completion;
        std::shared_ptr<ThumbnailGenerationAdmissionJobState> lifetime;
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished || m_childCount != 0 || m_retired) {
                return;
            }
            m_retired = true;
            completion = m_completion;
            lifetime = std::move(m_self);
        }
        completion.retire();
        Q_UNUSED(lifetime)
    }

    [[nodiscard]] bool acceptsCallbacks() const
    {
        const std::scoped_lock lock(m_mutex);
        return !m_logicalFinished && m_token != nullptr;
    }

    void publish(kiriview::ThumbnailGenerationResult result)
    {
        kiriview::ThumbnailGenerationCallback callback;
        QPointer<QObject> token;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                return;
            }
            m_logicalFinished = true;
            callback = std::move(m_callback);
            token = m_token;
        }

        if (callback) {
            callback(std::move(result));
        }
        if (token != nullptr) {
            delete token.data();
        }
        maybeRetire();
    }

    void storeWorker(kiriview::ImageWorkerTask worker)
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished) {
                m_workers.push_back(std::move(worker));
                return;
            }
        }
        worker.cancel();
    }

    void storeIoJob(kiriview::ImageIoJob ioJob)
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished) {
                m_ioJobs.push_back(std::move(ioJob));
                return;
            }
        }
        ioJob.cancel();
    }

    void storeAdmission(kiriview::ImageDecodeWorkspaceAdmission admission)
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished) {
                m_admission = std::move(admission);
                return;
            }
        }
        admission.cancel();
    }

    void startIdentityPreparation()
    {
        if (!beginChild()) {
            return;
        }
        const kiriview::ThumbnailGenerationRequest request = m_request;
        const kiriview::ThumbnailGenerationOriginalIdentityLoader loader
            = m_dependencies.openedCollectionOriginalIdentityLoader;
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        kiriview::ImageWorkerTask worker = m_workerScheduler.run(
            m_token,
            [request, loader]() {
                ThumbnailIdentityPreparationResult result;
                result.identity = loader(request, &result.errorString);
                return result;
            },
            [weakState](ThumbnailIdentityPreparationResult result) mutable {
                if (const auto state = weakState.lock()) {
                    state->identityPrepared(std::move(result));
                }
            });
        worker.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });
        storeWorker(std::move(worker));
    }

    void identityPrepared(ThumbnailIdentityPreparationResult result)
    {
        if (!acceptsCallbacks()) {
            return;
        }
        if (!result.identity.has_value()) {
            publish(failedResult(m_request.requestedBucket,
                result.errorString.isEmpty()
                    ? QStringLiteral("collection thumbnail identity failed")
                    : std::move(result.errorString)));
            return;
        }
        m_originalIdentity = std::move(*result.identity);
        if (!m_request.cacheInstallEnabled) {
            startSourceLoad();
            return;
        }
        startCacheLookup();
    }

    void startCacheLookup()
    {
        if (!beginChild()) {
            return;
        }
        kiriview::ThumbnailCacheLookupRequest lookupRequest;
        lookupRequest.localPathBytes = m_originalIdentity.localPathBytes;
        lookupRequest.originalIdentity = m_originalIdentity;
        lookupRequest.requestedBucket = m_request.requestedBucket;
        lookupRequest.workspacePriority = m_request.workspacePriority;
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        kiriview::ImageIoJob ioJob
            = m_dependencies.cacheLookupProvider(m_token, std::move(lookupRequest),
                [weakState](kiriview::ThumbnailCacheLookupResult result) mutable {
                    if (const auto state = weakState.lock()) {
                        state->cacheLookupFinished(std::move(result));
                    }
                });
        ioJob.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });
        storeIoJob(std::move(ioJob));
    }

    void cacheLookupFinished(kiriview::ThumbnailCacheLookupResult lookup)
    {
        if (!acceptsCallbacks()) {
            return;
        }
        switch (lookup.status) {
        case kiriview::ThumbnailCacheLookupStatus::Ready:
            publish(readyResultFromCache(lookup));
            return;
        case kiriview::ThumbnailCacheLookupStatus::Missing:
            startSourceLoad();
            return;
        case kiriview::ThumbnailCacheLookupStatus::ResourceLimitExceeded:
            publish(failedResult(m_request.requestedBucket, std::move(lookup.errorString),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
            return;
        case kiriview::ThumbnailCacheLookupStatus::Invalid:
        case kiriview::ThumbnailCacheLookupStatus::Failed:
            publish(failedResult(m_request.requestedBucket, std::move(lookup.errorString)));
            return;
        }
    }

    void startSourceLoad()
    {
        if (!beginChild()) {
            return;
        }
        const kiriview::ThumbnailGenerationRequest request = m_request;
        const kiriview::ThumbnailGenerationBytesLoader loader = m_dependencies.bytesLoader;
        const std::shared_ptr<kiriview::ImageSourceDataBudget> sourceDataBudget
            = m_dependencies.sourceDataBudget;
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        kiriview::ImageWorkerTask worker = m_workerScheduler.run(
            m_token,
            [request, loader, sourceDataBudget]() mutable {
                ThumbnailSourcePreparationResult result;
                result.sourceData = loader(request, &result.errorString);
                if (result.sourceData.data.isEmpty() && !result.errorString.isEmpty()) {
                    return result;
                }
                if (!result.sourceData.lease.isManaged()) {
                    result.sourceData.lease = sourceDataBudget->startLease();
                }
                const qsizetype reservedByteCount = result.sourceData.lease.reservedByteCount();
                if (result.sourceData.data.size() > reservedByteCount
                    && !result.sourceData.lease.tryReserve(
                        result.sourceData.data.size() - reservedByteCount)) {
                    result.sourceData = {};
                    result.errorString = kiriview::imageSourceDataResourceLimitDiagnostic();
                }
                return result;
            },
            [weakState](ThumbnailSourcePreparationResult result) mutable {
                if (const auto state = weakState.lock()) {
                    state->sourceLoaded(std::move(result));
                }
            });
        worker.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });
        storeWorker(std::move(worker));
    }

    void sourceLoaded(ThumbnailSourcePreparationResult result)
    {
        if (!acceptsCallbacks()) {
            return;
        }
        if (!result.errorString.isEmpty()) {
            publish(failedResult(m_request.requestedBucket, std::move(result.errorString)));
            return;
        }

        auto sourceData = std::make_shared<std::optional<kiriview::ImageSourceData>>(
            std::move(result.sourceData));
        kiriview::ImageDecodeDependencies decodeDependencies;
        decodeDependencies.dataLoader = [sourceData](QObject*, const kiriview::ImageDecodeRequest&,
                                            const kiriview::ImageDataCallback& loaded,
                                            const kiriview::ImageDataLoadErrorCallback&) mutable {
            if (sourceData->has_value() && loaded) {
                loaded(std::move(**sourceData));
                (*sourceData).reset();
            }
            return kiriview::ImageIoJob {};
        };
        decodeDependencies.dataPlanner = m_dependencies.imagePlanner;
        decodeDependencies.thumbnailPreviewLookupProvider =
            [](QObject*, const kiriview::ThumbnailCacheLookupRequest&,
                const kiriview::ThumbnailCacheLookupCallback&) { return kiriview::ImageIoJob {}; };
        decodeDependencies.workerScheduler = m_workerScheduler;
        decodeDependencies.sourceDataBudget = m_dependencies.sourceDataBudget;
        decodeDependencies.workspaceBudget = m_dependencies.workspaceBudget;

        QUrl sourceUrl = m_request.sourceUrl;
        if (sourceUrl.isEmpty() && !m_request.localPathBytes.isEmpty()) {
            sourceUrl = QUrl::fromLocalFile(QFile::decodeName(m_request.localPathBytes));
        }
        kiriview::DisplayedImageLocation location = m_request.openedCollectionScope.isEmpty()
            ? kiriview::DisplayedImageLocation::fromUrl(sourceUrl)
            : kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                  sourceUrl, m_request.openedCollectionScope);
        kiriview::ImageDecodeRequest decodeRequest
            = kiriview::ImageDecodeRequest::fromLocation(1, std::move(location),
                kiriview::ImageFirstDisplayDecodeContext {
                    QSize(m_maximumLongEdge, m_maximumLongEdge),
                });

        if (!beginChild()) {
            return;
        }
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        auto* decodeJob = new kiriview::ImageDecodeJob(m_token, std::move(decodeDependencies),
            kiriview::ImageDecodeJob::Callbacks {
                [weakState](const kiriview::ImageDecodeRequest&,
                    kiriview::DecodedImageResult decoded) mutable {
                    if (const auto state = weakState.lock()) {
                        state->decodeFinished(std::move(decoded));
                    }
                },
                [weakState](const kiriview::ImageDecodeRequest&,
                    kiriview::ImageDataLoadError error) mutable {
                    if (const auto state = weakState.lock()) {
                        QString errorString = std::visit(
                            [](const auto& failure) {
                                if constexpr (requires { failure.userMessage; }) {
                                    return failure.userMessage;
                                } else {
                                    return kiriview::mediaEntrySourceErrorText(failure);
                                }
                            },
                            error);
                        state->publish(
                            failedResult(state->m_request.requestedBucket, std::move(errorString)));
                    }
                },
                {},
                [weakState](const kiriview::ImageDecodeRequest&) {
                    if (const auto state = weakState.lock()) {
                        state->retireChild();
                    }
                },
            });
        bool abandonDecode = false;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                abandonDecode = true;
            } else {
                m_decodeJob = decodeJob;
            }
        }
        if (abandonDecode) {
            delete decodeJob;
            retireChild();
            return;
        }
        decodeJob->start(std::move(decodeRequest), std::nullopt, m_request.workspacePriority);
    }

    void decodeFinished(kiriview::DecodedImageResult result)
    {
        if (!acceptsCallbacks()) {
            return;
        }
        if (const kiriview::DecodedImageFailure* failure
            = kiriview::decodedImageResultFailure(result)) {
            const kiriview::ThumbnailGenerationStatus status
                = failure->cause == kiriview::DecodedImageFailureCause::ResourceLimitExceeded
                ? kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded
                : kiriview::ThumbnailGenerationStatus::Failed;
            publish(failedResult(m_request.requestedBucket, failure->diagnosticDetail, status));
            return;
        }
        kiriview::DecodedImage* decoded = kiriview::decodedImageResultImage(result);
        if (decoded == nullptr) {
            publish(failedResult(
                m_request.requestedBucket, QStringLiteral("image decode produced no image")));
            return;
        }

        const std::optional<ThumbnailTransformationPlan> plan
            = thumbnailTransformationPlan(*decoded, m_maximumLongEdge);
        const std::optional<qsizetype> additionalPeakByteCount = plan.has_value()
            ? thumbnailTransformationAdditionalPeakByteCount(*plan)
            : std::nullopt;
        if (!plan.has_value() || !additionalPeakByteCount.has_value()) {
            publish(failedResult(m_request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
            return;
        }

        const qsizetype baselineByteCount = decodedThumbnailWorkspaceByteCount(*decoded);
        auto retainedResult
            = std::make_shared<std::optional<kiriview::DecodedImageResult>>(std::move(result));
        const kiriview::ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *additionalPeakByteCount,
            baselineByteCount,
            m_request.workspacePriority,
        };
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        auto admission = m_dependencies.workspaceBudget->requestAdmission(m_token, admissionRequest,
            [weakState, retainedResult, baselineByteCount](
                kiriview::ImageDecodeWorkspaceLease lease) mutable {
                if (const auto state = weakState.lock()) {
                    state->transformationGranted(
                        std::move(*retainedResult), std::move(lease), baselineByteCount);
                    (*retainedResult).reset();
                }
            });
        if (!admission.has_value()) {
            publish(failedResult(m_request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
            return;
        }
        storeAdmission(std::move(*admission));
    }

    void transformationGranted(std::optional<kiriview::DecodedImageResult> retainedResult,
        kiriview::ImageDecodeWorkspaceLease lease, qsizetype baselineByteCount)
    {
        if (!acceptsCallbacks() || !retainedResult.has_value()) {
            return;
        }
        if (!beginChild()) {
            return;
        }
        const kiriview::ThumbnailGenerationRequest request = m_request;
        const kiriview::ThumbnailOriginalIdentity originalIdentity = m_originalIdentity;
        const int maximumLongEdge = m_maximumLongEdge;
        kiriview::ThumbnailGenerationDependencies dependencies = m_dependencies;
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        kiriview::ImageWorkerTask worker = m_workerScheduler.run(
            m_token,
            [request, originalIdentity, maximumLongEdge, dependencies = std::move(dependencies),
                result = std::move(*retainedResult), lease = std::move(lease),
                baselineByteCount]() mutable {
                std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> localBudget
                    = kiriview::prechargedImageDecodeWorkspaceBudget(
                        std::move(lease), baselineByteCount);
                if (localBudget == nullptr) {
                    return failedResult(request.requestedBucket,
                        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                        kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
                }
                dependencies.workspaceBudget = localBudget;
                kiriview::DecodedImage* decoded = kiriview::decodedImageResultImage(result);
                if (decoded == nullptr) {
                    localBudget->finalizePrechargedAdmission();
                    return failedResult(
                        request.requestedBucket, QStringLiteral("image decode produced no image"));
                }
                kiriview::ThumbnailGenerationImageDecodeResult rendered
                    = renderDecodedThumbnailImageImpl(*decoded, maximumLongEdge, localBudget);
                kiriview::ThumbnailGenerationResult generated;
                if (rendered.image.isNull()) {
                    const kiriview::ThumbnailGenerationStatus status = rendered.failureCause
                            == kiriview::DecodedImageFailureCause::ResourceLimitExceeded
                        ? kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded
                        : kiriview::ThumbnailGenerationStatus::Failed;
                    generated = failedResult(request.requestedBucket,
                        rendered.errorString.isEmpty()
                            ? QStringLiteral("thumbnail render produced no image")
                            : std::move(rendered.errorString),
                        status);
                } else {
                    generated = finishGeneratedThumbnailImage(request, originalIdentity,
                        std::move(rendered.workspaceHolds), std::move(rendered.transformationLease),
                        std::move(rendered.image), rendered.transformationOutputByteCount,
                        rendered.imageUsesTransformationReservation, dependencies);
                }
                localBudget->finalizePrechargedAdmission();
                return generated;
            },
            [weakState](kiriview::ThumbnailGenerationResult result) mutable {
                if (const auto state = weakState.lock()) {
                    state->publish(std::move(result));
                }
            });
        worker.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });
        storeWorker(std::move(worker));
    }

    void startLegacyAdmission()
    {
        const qsizetype peakByteCount = m_dependencies.workspaceBudget->perOperationByteLimit();
        const kiriview::ImageDecodeWorkspaceAdmissionRequest request {
            peakByteCount,
            0,
            m_request.workspacePriority,
        };
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        auto admission = m_dependencies.workspaceBudget->requestAdmission(
            m_token, request, [weakState](kiriview::ImageDecodeWorkspaceLease lease) mutable {
                if (const auto state = weakState.lock()) {
                    state->legacyAdmissionGranted(std::move(lease));
                }
            });
        if (!admission.has_value()) {
            publish(failedResult(m_request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
            return;
        }
        storeAdmission(std::move(*admission));
    }

    void legacyAdmissionGranted(kiriview::ImageDecodeWorkspaceLease lease)
    {
        if (!acceptsCallbacks() || !beginChild()) {
            return;
        }
        const kiriview::ThumbnailGenerationRequest request = m_request;
        kiriview::ThumbnailGenerationDependencies dependencies = m_dependencies;
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(shared_from_this());
        kiriview::ImageWorkerTask worker = m_workerScheduler.run(
            m_token,
            [request, dependencies = std::move(dependencies), lease = std::move(lease)]() mutable {
                std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> localBudget
                    = kiriview::prechargedImageDecodeWorkspaceBudget(std::move(lease));
                if (localBudget == nullptr) {
                    return failedResult(request.requestedBucket,
                        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                        kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
                }
                dependencies.workspaceBudget = localBudget;
                dependencies.cacheRepository.lookup = {};
                kiriview::ThumbnailGenerationResult result = generateThumbnailWithDependencies(
                    request, resolvedThumbnailGenerationDependencies(std::move(dependencies)));
                localBudget->finalizePrechargedAdmission();
                return result;
            },
            [weakState](kiriview::ThumbnailGenerationResult result) mutable {
                if (const auto state = weakState.lock()) {
                    state->publish(std::move(result));
                }
            });
        worker.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });
        storeWorker(std::move(worker));
    }

    QPointer<QObject> m_token;
    kiriview::ImageWorkerScheduler m_workerScheduler;
    kiriview::ThumbnailGenerationRequest m_request;
    kiriview::ThumbnailGenerationDependencies m_dependencies;
    kiriview::ThumbnailGenerationCallback m_callback;
    kiriview::ThumbnailOriginalIdentity m_originalIdentity;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::ImageDecodeWorkspaceAdmission m_admission;
    std::vector<kiriview::ImageWorkerTask> m_workers;
    std::vector<kiriview::ImageIoJob> m_ioJobs;
    QPointer<kiriview::ImageDecodeJob> m_decodeJob;
    std::shared_ptr<ThumbnailGenerationAdmissionJobState> m_self;
    int m_maximumLongEdge = 0;
    qsizetype m_childCount = 0;
    bool m_useLegacyDecoder = false;
    bool m_logicalFinished = false;
    bool m_retired = false;
    mutable std::mutex m_mutex;
};

class VideoThumbnailGenerationAdmissionJobState final
    : public std::enable_shared_from_this<VideoThumbnailGenerationAdmissionJobState>
{
public:
    VideoThumbnailGenerationAdmissionJobState(QObject* token,
        kiriview::ImageWorkerScheduler workerScheduler,
        kiriview::ThumbnailGenerationRequest request,
        kiriview::ThumbnailOriginalIdentity originalIdentity, int maximumLongEdge,
        kiriview::ThumbnailGenerationCallback callback,
        kiriview::ThumbnailGenerationDependencies dependencies)
        : m_token(token)
        , m_workerScheduler(std::move(workerScheduler))
        , m_request(std::move(request))
        , m_originalIdentity(std::move(originalIdentity))
        , m_maximumLongEdge(maximumLongEdge)
        , m_callback(std::move(callback))
        , m_dependencies(std::move(dependencies))
    {
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        const std::scoped_lock lock(m_mutex);
        m_completion = std::move(completion);
    }

    void start()
    {
        {
            const std::scoped_lock lock(m_mutex);
            m_self = shared_from_this();
        }
        const kiriview::ImageDecodeWorkspaceAdmissionRequest request {
            kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes,
            0,
            m_request.workspacePriority,
        };
        const std::weak_ptr<VideoThumbnailGenerationAdmissionJobState> weakState(
            shared_from_this());
        auto admission = m_dependencies.workspaceBudget->requestAdmission(
            m_token, request, [weakState](kiriview::ImageDecodeWorkspaceLease lease) mutable {
                if (const auto state = weakState.lock()) {
                    state->admissionGranted(std::move(lease));
                }
            });
        if (!admission.has_value()) {
            publish(failedResult(m_request.requestedBucket,
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded));
            return;
        }
        bool cancelAdmission = false;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                cancelAdmission = true;
            } else {
                m_admission = std::move(*admission);
            }
        }
        if (cancelAdmission) {
            admission->cancel();
        }
    }

    void cancel()
    {
        kiriview::ImageDecodeWorkspaceAdmission admission;
        kiriview::ImageIoJob providerJob;
        kiriview::ImageWorkerTask worker;
        std::optional<kiriview::ImageDecodeWorkspaceLease> unusedWorkingLease;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                return;
            }
            m_logicalFinished = true;
            m_callback = {};
            admission = std::move(m_admission);
            providerJob = std::move(m_providerJob);
            worker = std::move(m_worker);
            if (m_providerRetired) {
                unusedWorkingLease = std::move(m_workingLease);
                m_workingLease.reset();
            }
        }
        admission.cancel();
        providerJob.cancel();
        worker.cancel();
        unusedWorkingLease.reset();
        maybeRetire();
    }

private:
    [[nodiscard]] bool beginChild()
    {
        const std::scoped_lock lock(m_mutex);
        if (m_logicalFinished || m_token == nullptr) {
            return false;
        }
        ++m_childCount;
        return true;
    }

    void retireChild()
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (m_childCount > 0) {
                --m_childCount;
            }
        }
        maybeRetire();
    }

    void maybeRetire()
    {
        kiriview::ImageIoJobCompletion completion;
        std::shared_ptr<VideoThumbnailGenerationAdmissionJobState> lifetime;
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished || m_childCount != 0 || m_retired) {
                return;
            }
            m_retired = true;
            completion = m_completion;
            lifetime = std::move(m_self);
        }
        completion.retire();
        Q_UNUSED(lifetime)
    }

    [[nodiscard]] bool acceptsCallbacks() const
    {
        const std::scoped_lock lock(m_mutex);
        return !m_logicalFinished && m_token != nullptr;
    }

    void publish(kiriview::ThumbnailGenerationResult result)
    {
        kiriview::ThumbnailGenerationCallback callback;
        std::optional<kiriview::ImageDecodeWorkspaceLease> unusedWorkingLease;
        QPointer<QObject> token;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                return;
            }
            m_logicalFinished = true;
            callback = std::move(m_callback);
            if (m_providerRetired) {
                unusedWorkingLease = std::move(m_workingLease);
                m_workingLease.reset();
            }
            token = m_token;
        }
        unusedWorkingLease.reset();
        if (callback) {
            callback(std::move(result));
        }
        if (token != nullptr) {
            delete token.data();
        }
        maybeRetire();
    }

    void admissionGranted(kiriview::ImageDecodeWorkspaceLease lease)
    {
        if (!acceptsCallbacks() || !beginChild()) {
            return;
        }
        {
            const std::scoped_lock lock(m_mutex);
            m_workingLease.emplace(std::move(lease));
        }

        kiriview::VideoThumbnailExtractionRequest extractionRequest;
        extractionRequest.sourceUrl = m_request.sourceUrl;
        extractionRequest.maximumLongEdge = m_maximumLongEdge;
        const std::weak_ptr<VideoThumbnailGenerationAdmissionJobState> weakState(
            shared_from_this());
        kiriview::ImageIoJob providerJob
            = m_dependencies.videoExtractionProvider(m_token, std::move(extractionRequest),
                [weakState](kiriview::VideoThumbnailExtractionResult result) mutable {
                    if (const auto state = weakState.lock()) {
                        state->extractionFinished(std::move(result));
                    }
                });
        providerJob.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->providerRetired();
            }
        });

        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished) {
                m_providerJob = std::move(providerJob);
                return;
            }
        }
        providerJob.cancel();
    }

    void providerRetired()
    {
        std::optional<kiriview::ImageDecodeWorkspaceLease> unusedWorkingLease;
        QImage readyImage;
        QImage unusedReadyImage;
        {
            const std::scoped_lock lock(m_mutex);
            m_providerRetired = true;
            if (m_logicalFinished) {
                unusedWorkingLease = std::move(m_workingLease);
                m_workingLease.reset();
                unusedReadyImage = std::move(m_pendingReadyImage);
            } else {
                readyImage = std::move(m_pendingReadyImage);
            }
        }
        unusedReadyImage = {};
        unusedWorkingLease.reset();
        if (!readyImage.isNull()) {
            startReadyPostprocess(std::move(readyImage));
        }
        retireChild();
    }

    void extractionFinished(kiriview::VideoThumbnailExtractionResult result)
    {
        if (!acceptsCallbacks()) {
            return;
        }
        if (result.status == kiriview::VideoThumbnailExtractionStatus::Failed) {
            QString diagnostic;
            kiriview::ThumbnailGenerationStatus failureStatus
                = kiriview::ThumbnailGenerationStatus::Failed;
            if (result.failure.has_value()) {
                failureStatus = generationStatusForVideoExtractionFailure(result.failure->cause);
                diagnostic = std::move(result.failure->diagnostic);
            }
            publish(failedResult(m_request.requestedBucket,
                diagnostic.isEmpty() ? QStringLiteral("video thumbnail extraction failed")
                                     : std::move(diagnostic),
                failureStatus));
            return;
        }
        if (result.image.isNull() || result.failure.has_value()) {
            publish(failedResult(
                m_request.requestedBucket, QStringLiteral("video thumbnail produced no image")));
            return;
        }

        bool providerRetired = false;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_logicalFinished) {
                return;
            }
            providerRetired = m_providerRetired;
            if (!providerRetired) {
                m_pendingReadyImage = std::move(result.image);
            }
        }
        if (providerRetired) {
            startReadyPostprocess(std::move(result.image));
        }
    }

    void startReadyPostprocess(QImage image)
    {
        std::optional<kiriview::ImageDecodeWorkspaceLease> workingLease;
        {
            const std::scoped_lock lock(m_mutex);
            workingLease = std::move(m_workingLease);
            m_workingLease.reset();
        }
        if (!workingLease.has_value() || !beginChild()) {
            return;
        }
        const kiriview::ThumbnailGenerationRequest request = m_request;
        const kiriview::ThumbnailOriginalIdentity originalIdentity = m_originalIdentity;
        kiriview::ThumbnailGenerationDependencies dependencies = m_dependencies;
        const std::weak_ptr<VideoThumbnailGenerationAdmissionJobState> weakState(
            shared_from_this());
        kiriview::ImageWorkerTask worker = m_workerScheduler.run(
            m_token,
            [request, originalIdentity, dependencies = std::move(dependencies),
                image = std::move(image), lease = std::move(*workingLease)]() mutable {
                std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> localBudget
                    = kiriview::prechargedImageDecodeWorkspaceBudget(std::move(lease));
                if (localBudget == nullptr) {
                    return failedResult(request.requestedBucket,
                        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                        kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
                }
                dependencies.workspaceBudget = localBudget;
                kiriview::ImageDecodeWorkspaceLease localLease
                    = kiriview::ImageDecodeWorkspaceDetail::startLease(*localBudget);
                constexpr qsizetype workingByteCount
                    = kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes;
                const qsizetype outputByteCount = image.sizeInBytes();
                if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(localLease, workingByteCount)
                    || outputByteCount <= 0 || outputByteCount > workingByteCount
                    || !localLease.release(workingByteCount - outputByteCount)) {
                    localBudget->finalizePrechargedAdmission();
                    return failedResult(request.requestedBucket,
                        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                        kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
                }
                kiriview::ThumbnailGenerationResult generated = finishGeneratedThumbnailImage(
                    request, originalIdentity, {}, std::move(localLease), std::move(image),
                    outputByteCount, true, dependencies);
                localBudget->finalizePrechargedAdmission();
                return generated;
            },
            [weakState](kiriview::ThumbnailGenerationResult generated) mutable {
                if (const auto state = weakState.lock()) {
                    state->publish(std::move(generated));
                }
            });
        worker.setRetirementCallback([weakState]() {
            if (const auto state = weakState.lock()) {
                state->retireChild();
            }
        });

        {
            const std::scoped_lock lock(m_mutex);
            if (!m_logicalFinished) {
                m_worker = std::move(worker);
                return;
            }
        }
        worker.cancel();
    }

    QPointer<QObject> m_token;
    kiriview::ImageWorkerScheduler m_workerScheduler;
    kiriview::ThumbnailGenerationRequest m_request;
    kiriview::ThumbnailOriginalIdentity m_originalIdentity;
    int m_maximumLongEdge = 0;
    kiriview::ThumbnailGenerationCallback m_callback;
    kiriview::ThumbnailGenerationDependencies m_dependencies;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::ImageDecodeWorkspaceAdmission m_admission;
    kiriview::ImageIoJob m_providerJob;
    kiriview::ImageWorkerTask m_worker;
    std::optional<kiriview::ImageDecodeWorkspaceLease> m_workingLease;
    QImage m_pendingReadyImage;
    std::shared_ptr<VideoThumbnailGenerationAdmissionJobState> m_self;
    qsizetype m_childCount = 0;
    bool m_providerRetired = false;
    bool m_logicalFinished = false;
    bool m_retired = false;
    mutable std::mutex m_mutex;
};

kiriview::ImageIoJob startVideoThumbnailGenerationJob(QObject* receiver,
    kiriview::ThumbnailGenerationRequest request, kiriview::ThumbnailGenerationCallback callback,
    kiriview::ThumbnailGenerationDependencies dependencies,
    kiriview::ImageWorkerScheduler workerScheduler)
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

    if (receiver == nullptr) {
        if (callback) {
            callback(failedResult(request.requestedBucket,
                QStringLiteral("video thumbnail receiver is unavailable")));
        }
        return {};
    }

    auto* token = new QObject(receiver);
    auto state = std::make_shared<VideoThumbnailGenerationAdmissionJobState>(token,
        std::move(workerScheduler), std::move(request), std::move(originalIdentity),
        maximumLongEdge, std::move(callback), std::move(dependencies));
    kiriview::ImageIoJob job(
        token,
        [state](QObject* object) {
            state->cancel();
            object->deleteLater();
        },
        kiriview::ImageIoJobCancellationRetirement::Explicit);
    state->setCompletion(job.completion());
    const std::weak_ptr<VideoThumbnailGenerationAdmissionJobState> weakState(state);
    QObject::connect(
        token, &QObject::destroyed, token,
        [weakState](QObject*) {
            if (const auto activeState = weakState.lock()) {
                activeState->cancel();
            }
        },
        Qt::DirectConnection);
    state->start();
    return job;
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
        const bool useLegacyDecoder
            = static_cast<bool>(dependencies.imageDecoder) && !dependencies.imagePlanner;
        ThumbnailGenerationDependencies resolvedDependencies
            = resolvedThumbnailGenerationDependencies(dependencies, workerScheduler);
        if (request.sourceKind == ThumbnailSourceKind::DirectVideo) {
            return startVideoThumbnailGenerationJob(receiver, std::move(request),
                std::move(callback), std::move(resolvedDependencies), workerScheduler);
        }
        if (receiver == nullptr) {
            if (callback) {
                callback(generateThumbnail(request, std::move(resolvedDependencies)));
            }
            return ImageIoJob {};
        }

        auto* token = new QObject(receiver);
        auto state = std::make_shared<ThumbnailGenerationAdmissionJobState>(token, workerScheduler,
            std::move(request), std::move(resolvedDependencies), std::move(callback),
            useLegacyDecoder);
        ImageIoJob job(
            token,
            [state](QObject* object) {
                state->cancel();
                object->deleteLater();
            },
            ImageIoJobCancellationRetirement::Explicit);
        state->setCompletion(job.completion());
        const std::weak_ptr<ThumbnailGenerationAdmissionJobState> weakState(state);
        QObject::connect(
            token, &QObject::destroyed, token,
            [weakState](QObject*) {
                if (const auto activeState = weakState.lock()) {
                    activeState->cancel();
                }
            },
            Qt::DirectConnection);
        state->start();
        return job;
    };
}
}
