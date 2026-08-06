// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnailgeneration.h"

#include "archive/mediaentrysourcebackend.h"
#include "async/imageioworkerjob.h"
#include "bridge/rustqtconversion.h"
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

    return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage renderedThumbnailImage(
    const kiriview::DecodedImage& decoded, int maximumLongEdge, QString* errorString)
{
    return std::visit(
        [maximumLongEdge, errorString](const auto& image) -> QImage {
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
                return result.image;
            } else {
                return thumbnailFrame(image.firstFrame, maximumLongEdge);
            }
        },
        decoded);
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
            failure->errorString,
            failure->cause,
        };
    }

    kiriview::DecodedImage* decoded = kiriview::decodedImageResultImage(decodeResult);
    if (decoded == nullptr) {
        return {
            {},
            {},
            QStringLiteral("image decode produced no image"),
            kiriview::DecodedImageFailureCause::Unknown,
        };
    }

    kiriview::ThumbnailGenerationWorkspaceHolds workspaceHolds;
    if (auto* apng = std::get_if<kiriview::ApngAnimationImage>(decoded)) {
        const qsizetype transformationByteCount = apng->firstFrameWorkspaceHold.reservedByteCount();
        kiriview::ImageDecodeWorkspaceLease transformationLease = workspaceBudget->startLease();
        if (!transformationLease.tryReserve(transformationByteCount)) {
            return {
                {},
                {},
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
            };
        }
        kiriview::ImageDecodeWorkspaceHold transformationHold
            = transformationLease.retainOnly(transformationByteCount);
        if (!transformationHold.isManaged()) {
            return {
                {},
                {},
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
                kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
            };
        }
        workspaceHolds.decodedImage = std::move(apng->firstFrameWorkspaceHold);
        workspaceHolds.transformation = std::move(transformationHold);
    }

    QString errorString;
    QImage image = renderedThumbnailImage(*decoded, maximumLongEdge, &errorString);
    return {
        std::move(workspaceHolds),
        std::move(image),
        std::move(errorString),
        kiriview::DecodedImageFailureCause::Unknown,
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
    kiriview::ThumbnailGenerationWorkspaceHolds workspaceHolds, const QImage& image,
    const kiriview::ThumbnailGenerationDependencies& dependencies)
{
    QImage rgba8 = image.convertToFormat(QImage::Format_RGBA8888);
    if (rgba8.isNull()) {
        kiriview::ThumbnailGenerationResult failure = failedResult(
            request.requestedBucket, QStringLiteral("thumbnail image conversion failed"));
        failure.workspaceHolds = std::move(workspaceHolds);
        return failure;
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
        kiriview::ThumbnailGenerationResult failure
            = failedResult(install.requestedBucket, install.errorString);
        failure.workspaceHolds = std::move(workspaceHolds);
        return failure;
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
                                        QByteArray bytes, int maximumLongEdge) {
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
        std::move(decodeResult.workspaceHolds), decodeResult.image, dependencies);
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

    return kiriview::startThumbnailVideoExtractionJob(receiver, std::move(extractionRequest),
        [request = std::move(request), originalIdentity = std::move(originalIdentity),
            callback = std::move(callback), dependencies = std::move(dependencies)](
            kiriview::VideoThumbnailExtractionResult extractionResult) mutable {
            if (!callback) {
                return;
            }
            if (extractionResult.status == kiriview::VideoThumbnailExtractionStatus::Failed) {
                QString diagnostic;
                if (extractionResult.failure.has_value()) {
                    diagnostic = std::move(extractionResult.failure->diagnostic);
                }
                callback(failedResult(request.requestedBucket,
                    diagnostic.isEmpty() ? QStringLiteral("video thumbnail extraction failed")
                                         : std::move(diagnostic)));
                return;
            }
            if (extractionResult.image.isNull() || extractionResult.failure.has_value()) {
                callback(failedResult(
                    request.requestedBucket, QStringLiteral("video thumbnail produced no image")));
                return;
            }

            callback(finishGeneratedThumbnailImage(
                request, originalIdentity, {}, extractionResult.image, dependencies));
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
