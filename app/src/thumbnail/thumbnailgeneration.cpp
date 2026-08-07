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

kiriview::ThumbnailCacheLookupStatus thumbnailCacheStatus(
    kiriview::RustThumbnailCacheLookupStatus status)
{
    switch (status) {
    case kiriview::RustThumbnailCacheLookupStatus::Ready:
        return kiriview::ThumbnailCacheLookupStatus::Ready;
    case kiriview::RustThumbnailCacheLookupStatus::Missing:
        return kiriview::ThumbnailCacheLookupStatus::Missing;
    case kiriview::RustThumbnailCacheLookupStatus::Invalid:
        return kiriview::ThumbnailCacheLookupStatus::Invalid;
    case kiriview::RustThumbnailCacheLookupStatus::Failed:
        return kiriview::ThumbnailCacheLookupStatus::Failed;
    }

    return kiriview::ThumbnailCacheLookupStatus::Failed;
}

kiriview::ThumbnailCacheLookupResult lookupResultFromRust(
    const kiriview::RustThumbnailCacheLookupResult& rustResult)
{
    kiriview::ThumbnailCacheLookupResult result;
    result.status = thumbnailCacheStatus(rustResult.status);
    result.requestedBucket = thumbnailBucket(rustResult.requested_bucket);
    result.sourceBucket = thumbnailBucket(rustResult.source_bucket);
    result.sourceCachePath = kiriview::Bridge::qtString(rustResult.source_cache_path);
    result.errorString = kiriview::Bridge::qtString(rustResult.error);

    if (result.status != kiriview::ThumbnailCacheLookupStatus::Ready || rustResult.width <= 0
        || rustResult.height <= 0 || rustResult.stride <= 0) {
        return result;
    }

    const QByteArray pixels = kiriview::Bridge::qtByteArray(rustResult.pixels);
    const QImage image = kiriview::copiedImageFromBytes(pixels,
        QSize(rustResult.width, rustResult.height), rustResult.stride, QImage::Format_RGBA8888);
    if (image.isNull()) {
        result.status = kiriview::ThumbnailCacheLookupStatus::Failed;
        result.errorString = QStringLiteral("thumbnail cache RGBA8 result could not form a QImage");
        return result;
    }

    result.image = image;
    return result;
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
                kiriview::StaticImageDisplayDecodeResult result
                    = image.displayImage.refinementSource->decodeBlockingDisplayImage(
                        maximumLongEdge);
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
    struct TransformationTarget
    {
        QSize sourceSize;
        QSize targetSize;
        bool renderCreatesOutput = false;
        bool usesSmoothScale = false;
    };
    const TransformationTarget target = std::visit(
        [maximumLongEdge](const auto& image) -> TransformationTarget {
            using Image = std::decay_t<decltype(image)>;
            if constexpr (std::is_same_v<Image, kiriview::StaticDecodedImage>) {
                if (image.displayImage.refinementSource == nullptr) {
                    return {};
                }
                const QSize sourceSize = image.displayImage.refinementSource->imageSize();
                return { sourceSize, boundedSize(sourceSize, maximumLongEdge), true, false };
            } else {
                const QSize targetSize = boundedSize(image.firstFrame.size(), maximumLongEdge);
                const bool createsOutput = targetSize != image.firstFrame.size();
                return { image.firstFrame.size(), targetSize, createsOutput, createsOutput };
            }
        },
        decoded);
    const std::optional<qsizetype> outputByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(target.targetSize, 4, 1);
    const std::optional<qsizetype> transientByteCount = target.usesSmoothScale
        ? thumbnailSmoothScaleTransientByteCount(target.sourceSize, target.targetSize)
        : std::optional<qsizetype>(0);
    if (!outputByteCount.has_value() || !transientByteCount.has_value()) {
        return std::nullopt;
    }
    return ThumbnailTransformationPlan {
        *outputByteCount,
        *transientByteCount,
        target.renderCreatesOutput,
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

    kiriview::ThumbnailGenerationWorkspaceHolds workspaceHolds;
    kiriview::ImageDecodeWorkspaceHold decodedImageWorkspaceHold = std::visit(
        [](auto& image) -> kiriview::ImageDecodeWorkspaceHold {
            if constexpr (requires { image.firstFrameWorkspaceHold; }) {
                return image.firstFrameWorkspaceHold;
            }
            return {};
        },
        *decoded);
    const std::optional<ThumbnailTransformationPlan> transformationPlan
        = thumbnailTransformationPlan(*decoded, maximumLongEdge);
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
        = workspaceBudget->startLeaseForOperation(decodedImageWorkspaceHold.reservedByteCount());
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
        image = renderedThumbnailImage(*decoded, maximumLongEdge, &errorString, &failureCause);
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
    kiriview::ActiveNavigationThumbnailDemandBucket requestedBucket)
{
    kiriview::RustThumbnailCacheLookupResult rustResult;
    if (identity.isNonFileUri()) {
        const QByteArray uri = identity.uri.toUtf8();
        const QByteArray mimeType = identity.mimeType.toUtf8();
        rustResult = kiriview::rustLookupDisplayThumbnailNonFileUriRgba8(
            kiriview::Bridge::rustStr(uri), identity.mtimeSeconds, identity.originalByteSize,
            kiriview::Bridge::rustStr(mimeType), rustBucket(requestedBucket));
    } else {
        rustResult = kiriview::rustLookupDisplayThumbnailRgba8(
            kiriview::Bridge::rustBytes(identity.localPathBytes), rustBucket(requestedBucket));
    }
    return lookupResultFromRust(rustResult);
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
    workspaceHolds.transformation = transformationLease.retainOnly(transformationOutputByteCount);
    if (!workspaceHolds.transformation.isManaged()) {
        rgba8 = {};
        workspaceHolds = {};
        return failedResult(request.requestedBucket,
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
            kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    }

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
        rgba8 = {};
        workspaceHolds = {};
        return failedResult(install.requestedBucket, install.errorString);
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
        dependencies.cacheRepository.lookup = defaultThumbnailGenerationCacheLookup;
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

    kiriview::ThumbnailVideoExtractionProvider extractionProvider
        = std::move(dependencies.videoExtractionProvider);
    return extractionProvider(receiver, std::move(extractionRequest),
        [request = std::move(request), originalIdentity = std::move(originalIdentity),
            callback = std::move(callback), dependencies = std::move(dependencies)](
            kiriview::VideoThumbnailExtractionResult extractionResult) mutable {
            if (!callback) {
                return;
            }
            if (extractionResult.status == kiriview::VideoThumbnailExtractionStatus::Failed) {
                QString diagnostic;
                kiriview::ThumbnailGenerationStatus failureStatus
                    = kiriview::ThumbnailGenerationStatus::Failed;
                if (extractionResult.failure.has_value()) {
                    if (extractionResult.failure->cause
                        == kiriview::VideoThumbnailExtractionFailureCause::ResourceLimit) {
                        failureStatus = kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded;
                    }
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

            callback(finishGeneratedThumbnailImage(request, originalIdentity, {}, {},
                std::move(extractionResult.image), 0, false, dependencies));
        });
}
}

namespace kiriview {
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
