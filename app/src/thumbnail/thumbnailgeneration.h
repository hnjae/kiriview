// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_THUMBNAILGENERATION_H
#define KIRIVIEW_THUMBNAILGENERATION_H

#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "decoding/decodedimagefailure.h"
#include "decoding/imagedecodeworkspace.h"
#include "decoding/imagesourcedata.h"
#include "location/imagelocation.h"
#include "thumbnail/thumbnailbucket.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/thumbnailoriginalidentity.h"
#include "thumbnail/thumbnailsourcekind.h"
#include "thumbnail/videothumbnailextractionadapter.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

class QObject;

namespace kiriview {
enum class ThumbnailGenerationStatus {
    Ready,
    VideoExtractionInvalidRequest,
    VideoSourceUnavailable,
    VideoUnsupportedMedia,
    VideoBackendFailure,
    VideoExtractionTimedOut,
    VideoNoRepresentativeImage,
    ResourceLimitExceeded,
    Failed,
};

enum class ThumbnailGenerationDiagnosticKind {
    None,
    CacheInstallFailed,
};

struct ThumbnailGenerationWorkspaceHolds
{
    ImageDecodeWorkspaceHold decodedImage;
    ImageDecodeWorkspaceHold transformation;
};

struct ThumbnailGenerationRequest
{
    QByteArray localPathBytes;
    ThumbnailOriginalIdentity originalIdentity;
    OpenedCollectionScopeLocation openedCollectionScope;
    QUrl sourceUrl;
    QString sourceLabel;
    ThumbnailSourceKind sourceKind = ThumbnailSourceKind::DirectImage;
    ActiveNavigationThumbnailDemandBucket requestedBucket
        = ActiveNavigationThumbnailDemandBucket::None;
    bool cacheInstallEnabled = true;
};

struct ThumbnailGenerationResult
{
    ThumbnailGenerationStatus status = ThumbnailGenerationStatus::Failed;
    ThumbnailGenerationWorkspaceHolds workspaceHolds;
    QImage image;
    ActiveNavigationThumbnailDemandBucket requestedBucket
        = ActiveNavigationThumbnailDemandBucket::None;
    QString installedCachePath;
    QString errorString;
    ThumbnailGenerationDiagnosticKind diagnosticKind = ThumbnailGenerationDiagnosticKind::None;

    ThumbnailGenerationResult() = default;
    ThumbnailGenerationResult(ThumbnailGenerationStatus generationStatus,
        ThumbnailGenerationWorkspaceHolds retainedWorkspace, QImage generatedImage,
        ActiveNavigationThumbnailDemandBucket bucket, QString cachePath = {},
        QString diagnostic = {},
        ThumbnailGenerationDiagnosticKind resultDiagnosticKind
        = ThumbnailGenerationDiagnosticKind::None)
        : status(generationStatus)
        , workspaceHolds(std::move(retainedWorkspace))
        , image(std::move(generatedImage))
        , requestedBucket(bucket)
        , installedCachePath(std::move(cachePath))
        , errorString(std::move(diagnostic))
        , diagnosticKind(resultDiagnosticKind)
    {
    }
    ~ThumbnailGenerationResult() = default;
    ThumbnailGenerationResult(const ThumbnailGenerationResult&) = default;
    ThumbnailGenerationResult(ThumbnailGenerationResult&&) noexcept = default;

    ThumbnailGenerationResult& operator=(const ThumbnailGenerationResult& other)
    {
        if (this != &other) {
            ThumbnailGenerationResult next(other);
            swap(*this, next);
        }
        return *this;
    }

    ThumbnailGenerationResult& operator=(ThumbnailGenerationResult&& other) noexcept
    {
        if (this != &other) {
            ThumbnailGenerationResult next(std::move(other));
            swap(*this, next);
        }
        return *this;
    }

    friend void swap(ThumbnailGenerationResult& left, ThumbnailGenerationResult& right) noexcept
    {
        using std::swap;
        swap(left.status, right.status);
        swap(left.workspaceHolds, right.workspaceHolds);
        swap(left.image, right.image);
        swap(left.requestedBucket, right.requestedBucket);
        swap(left.installedCachePath, right.installedCachePath);
        swap(left.errorString, right.errorString);
        swap(left.diagnosticKind, right.diagnosticKind);
    }
};

using ThumbnailGenerationBytesLoader
    = std::function<ImageSourceData(const ThumbnailGenerationRequest&, QString*)>;
using ThumbnailGenerationOriginalIdentityLoader
    = std::function<std::optional<ThumbnailOriginalIdentity>(
        const ThumbnailGenerationRequest&, QString*)>;

struct ThumbnailGenerationImageDecodeResult
{
    ThumbnailGenerationImageDecodeResult() = default;
    ThumbnailGenerationImageDecodeResult(ThumbnailGenerationWorkspaceHolds retainedWorkspace,
        ImageDecodeWorkspaceLease pendingTransformation, QImage decodedImage,
        QString diagnostic = {}, DecodedImageFailureCause cause = DecodedImageFailureCause::Unknown,
        qsizetype outputByteCount = 0, bool imageUsesTransformation = false)
        : workspaceHolds(std::move(retainedWorkspace))
        , transformationLease(std::move(pendingTransformation))
        , image(std::move(decodedImage))
        , errorString(std::move(diagnostic))
        , failureCause(cause)
        , transformationOutputByteCount(outputByteCount)
        , imageUsesTransformationReservation(imageUsesTransformation)
    {
    }
    ThumbnailGenerationImageDecodeResult(const ThumbnailGenerationImageDecodeResult&) = delete;
    ThumbnailGenerationImageDecodeResult& operator=(const ThumbnailGenerationImageDecodeResult&)
        = delete;
    ~ThumbnailGenerationImageDecodeResult() = default;
    ThumbnailGenerationImageDecodeResult(ThumbnailGenerationImageDecodeResult&&) noexcept = default;
    ThumbnailGenerationImageDecodeResult& operator=(
        ThumbnailGenerationImageDecodeResult&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        ThumbnailGenerationWorkspaceHolds nextWorkspaceHolds = std::move(other.workspaceHolds);
        ImageDecodeWorkspaceLease nextTransformationLease = std::move(other.transformationLease);
        QImage nextImage = std::move(other.image);
        image = {};
        transformationLease = {};
        workspaceHolds = {};
        workspaceHolds = std::move(nextWorkspaceHolds);
        transformationLease = std::move(nextTransformationLease);
        image = std::move(nextImage);
        errorString = std::move(other.errorString);
        failureCause = other.failureCause;
        transformationOutputByteCount = other.transformationOutputByteCount;
        imageUsesTransformationReservation = other.imageUsesTransformationReservation;
        return *this;
    }

    ThumbnailGenerationWorkspaceHolds workspaceHolds;
    ImageDecodeWorkspaceLease transformationLease;
    QImage image;
    QString errorString;
    DecodedImageFailureCause failureCause = DecodedImageFailureCause::Unknown;
    qsizetype transformationOutputByteCount = 0;
    bool imageUsesTransformationReservation = false;
};

using ThumbnailGenerationImageDecoder
    = std::function<ThumbnailGenerationImageDecodeResult(QByteArray, int)>;
using ThumbnailGenerationMaximumLongEdgePolicy
    = std::function<int(ActiveNavigationThumbnailDemandBucket)>;

struct ThumbnailGenerationCacheInstallResult
{
    bool success = false;
    ActiveNavigationThumbnailDemandBucket requestedBucket
        = ActiveNavigationThumbnailDemandBucket::None;
    QString installedCachePath;
    QString errorString;
};

using ThumbnailGenerationCacheLookup = std::function<std::optional<ThumbnailCacheLookupResult>(
    const ThumbnailOriginalIdentity&, ActiveNavigationThumbnailDemandBucket)>;
using ThumbnailGenerationCacheInstall = std::function<ThumbnailGenerationCacheInstallResult(
    const ThumbnailOriginalIdentity&, ActiveNavigationThumbnailDemandBucket, const QImage&)>;

struct ThumbnailGenerationCacheRepository
{
    ThumbnailGenerationCacheLookup lookup;
    ThumbnailGenerationCacheInstall install;
};

struct ThumbnailGenerationDependencies
{
    ThumbnailGenerationBytesLoader bytesLoader;
    ThumbnailGenerationImageDecoder imageDecoder;
    ThumbnailGenerationMaximumLongEdgePolicy maximumLongEdgeForBucket;
    ThumbnailGenerationOriginalIdentityLoader openedCollectionOriginalIdentityLoader;
    ThumbnailGenerationCacheRepository cacheRepository;
    ThumbnailVideoExtractionProvider videoExtractionProvider;
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
};

using ThumbnailGenerationCallback = std::function<void(ThumbnailGenerationResult)>;
using ThumbnailGenerationProvider
    = std::function<ImageIoJob(QObject*, ThumbnailGenerationRequest, ThumbnailGenerationCallback)>;

ThumbnailGenerationResult generateThumbnail(
    const ThumbnailGenerationRequest& request, ThumbnailGenerationDependencies dependencies = {});

ThumbnailGenerationProvider defaultThumbnailGenerationProvider(
    ImageWorkerScheduler workerScheduler = {}, ThumbnailGenerationDependencies dependencies = {});
}

#endif
