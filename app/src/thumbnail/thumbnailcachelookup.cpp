// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnailcachelookup.h"

#include "bridge/rustqtconversion.h"
#include "decoding/imagerendering.h"
#include "kiriview/src/support/thumbnailcache.cxx.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <Qt>
#include <cstdint>
#include <mutex>
#include <new>
#include <utility>

namespace {
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

kiriview::ThumbnailCacheLookupStatus thumbnailStatus(
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

kiriview::ThumbnailCacheLookupResult unadmittedThumbnailCacheLookup(
    const kiriview::ThumbnailCacheLookupRequest& request)
{
    kiriview::RustThumbnailCacheLookupResult rustResult;
    if (request.originalIdentity.isNonFileUri()) {
        const QByteArray uri = request.originalIdentity.uri.toUtf8();
        const QByteArray mimeType = request.originalIdentity.mimeType.toUtf8();
        rustResult
            = kiriview::rustLookupDisplayThumbnailNonFileUriRgba8(kiriview::Bridge::rustStr(uri),
                request.originalIdentity.mtimeSeconds, request.originalIdentity.originalByteSize,
                kiriview::Bridge::rustStr(mimeType), rustBucket(request.requestedBucket));
    } else {
        const QByteArray localPathBytes = request.originalIdentity.localPathBytes.isEmpty()
            ? request.localPathBytes
            : request.originalIdentity.localPathBytes;
        rustResult = kiriview::rustLookupDisplayThumbnailRgba8(
            kiriview::Bridge::rustBytes(localPathBytes), rustBucket(request.requestedBucket));
    }

    kiriview::ThumbnailCacheLookupResult result;
    result.status = thumbnailStatus(rustResult.status);
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

std::optional<qsizetype> thumbnailCacheLookupPeakByteCount()
{
    constexpr QSize maximumCacheRasterSize(1024, 1024);
    // Six full-size RGBA buffers conservatively cover the validated decoder canvas, normalized
    // pixels, downscaler input/output and floating-point workspace, and the Rust-to-Qt copies.
    return kiriview::checkedImageDecodeWorkspaceByteCount(maximumCacheRasterSize, 4, 6);
}

kiriview::ThumbnailCacheLookupResult thumbnailCacheLookupResourceLimitResult(
    const kiriview::ThumbnailCacheLookupRequest& request)
{
    return {
        kiriview::ThumbnailCacheLookupStatus::ResourceLimitExceeded,
        {},
        request.requestedBucket,
        {},
        {},
        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
    };
}

kiriview::ThumbnailCacheLookupResult admittedThumbnailCacheLookup(
    const kiriview::ThumbnailCacheLookupRequest& request,
    kiriview::ImageDecodeWorkspaceLease workspaceLease)
{
    const std::optional<qsizetype> peakByteCount = thumbnailCacheLookupPeakByteCount();
    if (!peakByteCount.has_value() || !workspaceLease.isManaged()
        || workspaceLease.reservedByteCount() < *peakByteCount) {
        return thumbnailCacheLookupResourceLimitResult(request);
    }

    kiriview::ThumbnailCacheLookupResult result;
    try {
        result = unadmittedThumbnailCacheLookup(request);
    } catch (const std::bad_alloc&) {
        return thumbnailCacheLookupResourceLimitResult(request);
    }
    if (result.status != kiriview::ThumbnailCacheLookupStatus::Ready) {
        return result;
    }

    result.image = kiriview::displayReadyImageRetainingDecodeWorkspace(
        std::move(result.image), std::move(workspaceLease));
    if (result.image.isNull()) {
        result = thumbnailCacheLookupResourceLimitResult(request);
    }
    return result;
}

class ThumbnailCacheLookupAdmissionJobState final
{
public:
    ThumbnailCacheLookupAdmissionJobState(QObject* token,
        kiriview::ImageWorkerScheduler workerScheduler,
        kiriview::ThumbnailCacheLookupRequest request,
        kiriview::ThumbnailCacheLookupCallback callback)
        : m_token(token)
        , m_workerScheduler(std::move(workerScheduler))
        , m_request(std::move(request))
        , m_callback(std::move(callback))
    {
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        const std::scoped_lock lock(m_mutex);
        m_completion = std::move(completion);
    }

    [[nodiscard]] kiriview::ImageDecodeWorkspacePriority workspacePriority() const
    {
        const std::scoped_lock lock(m_mutex);
        return m_request.workspacePriority;
    }

    void setAdmission(kiriview::ImageDecodeWorkspaceAdmission admission)
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (m_phase == Phase::WaitingForAdmission) {
                m_admission = std::move(admission);
                return;
            }
        }
        admission.cancel();
    }

    void rejectAdmission()
    {
        kiriview::ImageIoJobCompletion completion;
        kiriview::ThumbnailCacheLookupCallback callback;
        kiriview::ThumbnailCacheLookupResult result;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_phase != Phase::WaitingForAdmission) {
                return;
            }
            m_phase = Phase::Finished;
            completion = m_completion;
            callback = std::move(m_callback);
            result = thumbnailCacheLookupResourceLimitResult(m_request);
        }
        completion.claimAndDelete(
            [callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) {
                    callback(std::move(result));
                }
            });
        completion.retire();
    }

    void grant(kiriview::ImageDecodeWorkspaceLease workspaceLease)
    {
        kiriview::ImageWorkerScheduler workerScheduler;
        kiriview::ThumbnailCacheLookupRequest request;
        kiriview::ThumbnailCacheLookupCallback callback;
        kiriview::ImageIoJobCompletion completion;
        QObject* token = nullptr;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_phase != Phase::WaitingForAdmission) {
                return;
            }
            m_phase = Phase::StartingWorker;
            m_admission = {};
            workerScheduler = m_workerScheduler;
            request = std::move(m_request);
            callback = std::move(m_callback);
            completion = m_completion;
            token = m_token.data();
        }

        if (token == nullptr || !completion.isActive()) {
            {
                const std::scoped_lock lock(m_mutex);
                m_phase = Phase::Finished;
            }
            completion.retire();
            return;
        }

        kiriview::ImageWorkerTask worker = workerScheduler.run(
            token,
            [request = std::move(request), workspaceLease = std::move(workspaceLease)]() mutable {
                return admittedThumbnailCacheLookup(request, std::move(workspaceLease));
            },
            [completion, callback = std::move(callback)](
                kiriview::ThumbnailCacheLookupResult result) mutable {
                completion.claimAndDelete([&callback, result = std::move(result)]() mutable {
                    if (callback) {
                        callback(std::move(result));
                    }
                });
            });
        worker.setRetirementCallback([completion]() { completion.retire(); });

        {
            const std::scoped_lock lock(m_mutex);
            if (m_phase == Phase::StartingWorker) {
                m_phase = Phase::WorkerSubmitted;
                m_worker = std::move(worker);
                return;
            }
        }
        worker.cancel();
    }

    void cancel()
    {
        kiriview::ImageDecodeWorkspaceAdmission admission;
        kiriview::ImageWorkerTask worker;
        kiriview::ImageIoJobCompletion completion;
        bool retireNow = false;
        {
            const std::scoped_lock lock(m_mutex);
            completion = m_completion;
            switch (m_phase) {
            case Phase::WaitingForAdmission:
                admission = std::move(m_admission);
                retireNow = true;
                break;
            case Phase::StartingWorker:
                break;
            case Phase::WorkerSubmitted:
                worker = std::move(m_worker);
                break;
            case Phase::Finished:
            case Phase::Canceled:
                return;
            }
            m_phase = Phase::Canceled;
            m_callback = {};
        }

        admission.cancel();
        worker.cancel();
        if (retireNow) {
            completion.retire();
        }
    }

private:
    enum class Phase {
        WaitingForAdmission,
        StartingWorker,
        WorkerSubmitted,
        Finished,
        Canceled,
    };

    QPointer<QObject> m_token;
    kiriview::ImageWorkerScheduler m_workerScheduler;
    kiriview::ThumbnailCacheLookupRequest m_request;
    kiriview::ThumbnailCacheLookupCallback m_callback;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::ImageDecodeWorkspaceAdmission m_admission;
    kiriview::ImageWorkerTask m_worker;
    Phase m_phase = Phase::WaitingForAdmission;
    mutable std::mutex m_mutex;
};
}

namespace kiriview {
ThumbnailCacheLookupResult lookupThumbnailCache(const ThumbnailCacheLookupRequest& request,
    const std::shared_ptr<ImageDecodeWorkspaceBudget>& workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        return ThumbnailCacheLookupResult {
            ThumbnailCacheLookupStatus::ResourceLimitExceeded,
            {},
            request.requestedBucket,
            {},
            {},
            imageDecodeWorkspaceResourceLimitDiagnostic(),
        };
    }

    const std::optional<qsizetype> peakByteCount = thumbnailCacheLookupPeakByteCount();
    ImageDecodeWorkspaceLease workspaceLease
        = ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    if (!peakByteCount.has_value()
        || !ImageDecodeWorkspaceDetail::tryReserve(workspaceLease, *peakByteCount)) {
        return thumbnailCacheLookupResourceLimitResult(request);
    }
    return admittedThumbnailCacheLookup(request, std::move(workspaceLease));
}

ThumbnailCacheLookupProvider defaultThumbnailCacheLookupProvider(
    ImageWorkerScheduler workerScheduler,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    return [workerScheduler = std::move(workerScheduler),
               workspaceBudget = std::move(workspaceBudget)](QObject* receiver,
               ThumbnailCacheLookupRequest request, ThumbnailCacheLookupCallback callback) {
        if (receiver == nullptr) {
            if (callback) {
                callback(lookupThumbnailCache(request, workspaceBudget));
            }
            return ImageIoJob {};
        }

        auto* token = new QObject(receiver);
        auto state = std::make_shared<ThumbnailCacheLookupAdmissionJobState>(
            token, workerScheduler, std::move(request), std::move(callback));
        ImageIoJob job(
            token,
            [state](QObject* object) {
                state->cancel();
                object->deleteLater();
            },
            ImageIoJobCancellationRetirement::Explicit);
        state->setCompletion(job.completion());
        const std::weak_ptr<ThumbnailCacheLookupAdmissionJobState> weakState(state);
        QObject::connect(
            token, &QObject::destroyed, token,
            [weakState](QObject*) {
                if (const auto activeState = weakState.lock()) {
                    activeState->cancel();
                }
            },
            Qt::DirectConnection);

        const std::optional<qsizetype> peakByteCount = thumbnailCacheLookupPeakByteCount();
        if (!peakByteCount.has_value()) {
            state->rejectAdmission();
            return job;
        }
        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *peakByteCount,
            0,
            state->workspacePriority(),
        };
        auto admission = workspaceBudget->requestAdmission(
            token, admissionRequest, [weakState](ImageDecodeWorkspaceLease workspaceLease) mutable {
                if (const auto activeState = weakState.lock()) {
                    activeState->grant(std::move(workspaceLease));
                }
            });
        if (!admission.has_value()) {
            state->rejectAdmission();
            return job;
        }
        state->setAdmission(std::move(*admission));
        return job;
    };
}
}
