// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourceruntime.h"

#include "async/imagecallback.h"
#include "async/imageioworkerjob.h"
#include "mediaentrysourcebackend_p.h"
#include "mediaentrysourcerunner.h"

#include <optional>
#include <utility>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;

void finishMediaEntrySourceCandidateResult(const kiriview::MediaEntrySourceCandidatesResult& result,
    const kiriview::ImageDocumentPageCandidatesCallback& callback,
    const kiriview::MediaEntrySourceErrorCallback& errorCallback)
{
    if (const auto* error = kiriview::mediaEntrySourceResultError(result)) {
        kiriview::invokeIfSet(errorCallback, *error);
        return;
    }

    const auto* candidates = kiriview::mediaEntrySourceResultValue(result);
    if (candidates != nullptr) {
        kiriview::invokeIfSet(callback, candidates->candidates);
    }
}

void finishMediaEntrySourceDataResult(kiriview::MediaEntrySourceImageDataResult result,
    kiriview::ImageDataCallback callback, kiriview::MediaEntrySourceErrorCallback errorCallback)
{
    if (const auto* error = kiriview::mediaEntrySourceResultError(result)) {
        kiriview::invokeIfSet(errorCallback, *error);
        return;
    }

    auto* data = kiriview::mediaEntrySourceResultValue(result);
    if (data != nullptr) {
        kiriview::invokeIfSet(
            callback, kiriview::ImageSourceData(std::move(data->data), std::move(data->lease)));
    }
}

kiriview::MediaEntrySourceError nonCurrentOpenedCollectionScopeError(
    kiriview::MediaEntrySourceOperation operation,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    return Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
        kiriview::MediaEntrySourceBackendKind::Unknown, operation, openedCollectionScope,
        QStringLiteral("requested collection is not the current media entry source"));
}

}

namespace kiriview {
MediaEntrySourceRuntime::MediaEntrySourceRuntime(QObject* context,
    MediaEntrySourceFactory sourceFactory, ImageWorkerScheduler workerScheduler,
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget)
    : m_context(context)
    , m_sourceFactory(std::move(sourceFactory))
    , m_workerScheduler(std::move(workerScheduler))
    , m_sourceDataBudget(sourceDataBudget != nullptr ? std::move(sourceDataBudget)
                                                     : defaultImageSourceDataBudget())
{
}

MediaEntrySourceRuntime::~MediaEntrySourceRuntime() { clear(); }

void MediaEntrySourceRuntime::clear()
{
    cancelCandidateLoadBatch();
    m_sourceGeneration.invalidate();
    m_runner.reset();
}

void MediaEntrySourceRuntime::switchToOpenedCollectionScope(
    OpenedCollectionScopeLocation openedCollectionScope)
{
    if (openedCollectionScope.isEmpty()) {
        clear();
        return;
    }
    if (hasCurrentOpenedCollectionScope(openedCollectionScope)) {
        return;
    }

    cancelCandidateLoadBatch();
    m_sourceGeneration.invalidate();
    m_runner = std::make_shared<MediaEntrySourceRunner>(
        std::move(openedCollectionScope), m_sourceFactory);
}

bool MediaEntrySourceRuntime::hasCurrentOpenedCollectionScope() const
{
    return m_runner != nullptr && !m_runner->openedCollectionScope().isEmpty();
}

bool MediaEntrySourceRuntime::hasCurrentOpenedCollectionScope(
    const OpenedCollectionScopeLocation& openedCollectionScope) const
{
    return hasCurrentOpenedCollectionScope()
        && sameOpenedCollectionScopeSnapshot(
            m_runner->openedCollectionScope(), openedCollectionScope);
}

ImageIoJob MediaEntrySourceRuntime::loadOpenedCollectionCandidates(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback)
{
    const OpenedCollectionScopeLocation requestedOpenedCollectionScope = openedCollectionScope;
    switchToOpenedCollectionScope(std::move(openedCollectionScope));
    if (m_runner == nullptr) {
        invokeIfSet(errorCallback,
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                requestedOpenedCollectionScope,
                QStringLiteral("media entry source runtime has no collection runner")));
        return ImageIoJob();
    }

    if (receiver != nullptr && m_candidateLoadState.batchInProgress()) {
        return m_candidateLoadState.addLoad(
            receiver, std::move(callback), std::move(errorCallback));
    }

    if (const std::optional<std::vector<ImageDocumentPageCandidate>> cachedCandidates
        = m_runner->cachedImageDocumentPageCandidates()) {
        invokeIfSet(callback, *cachedCandidates);
        return ImageIoJob();
    }

    if (receiver == nullptr) {
        finishMediaEntrySourceCandidateResult(
            m_runner->loadImageDocumentPageCandidates(), callback, errorCallback);
        return ImageIoJob();
    }

    ImageIoJob ioJob
        = m_candidateLoadState.addLoad(receiver, std::move(callback), std::move(errorCallback));
    if (const std::optional<MediaEntrySourceCandidateLoadBatch> batch
        = m_candidateLoadState.startBatch()) {
        startCandidateLoad(*batch);
    }

    return ioJob;
}

ImageIoJob MediaEntrySourceRuntime::loadOpenedCollectionImageData(QObject* receiver,
    const ImageDecodeRequest& request, ImageDataCallback callback,
    MediaEntrySourceErrorCallback errorCallback)
{
    const OpenedCollectionScopeLocation& requestedOpenedCollectionScope
        = request.openedCollectionScope();
    if (!hasCurrentOpenedCollectionScope(requestedOpenedCollectionScope)) {
        invokeIfSet(errorCallback,
            nonCurrentOpenedCollectionScopeError(
                MediaEntrySourceOperation::ReadImageData, requestedOpenedCollectionScope));
        return ImageIoJob();
    }

    if (receiver == nullptr) {
        finishMediaEntrySourceDataResult(
            m_runner->loadImageData(request.imageUrl(), m_sourceDataBudget->startLease()),
            std::move(callback), std::move(errorCallback));
        return ImageIoJob();
    }

    const quint64 generation = m_sourceGeneration.current();
    const QUrl& imageUrl = request.imageUrl();
    std::shared_ptr<MediaEntrySourceRunner> runner = m_runner;
    ImageSourceDataLease lease = m_sourceDataBudget->startLease();

    return startImageIoWorkerJob(
        m_context, receiver, m_workerScheduler,
        [runner = std::move(runner), imageUrl, lease = std::move(lease)]() mutable {
            return runner->loadImageData(imageUrl, std::move(lease));
        },
        [generation, this, callback = std::move(callback),
            errorCallback = std::move(errorCallback)](
            MediaEntrySourceImageDataResult result) mutable {
            if (!m_sourceGeneration.accepts(generation)) {
                return;
            }

            finishMediaEntrySourceDataResult(
                std::move(result), std::move(callback), std::move(errorCallback));
        });
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceRuntime::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl)
{
    if (!hasCurrentOpenedCollectionScope(openedCollectionScope)) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
            nonCurrentOpenedCollectionScopeError(
                MediaEntrySourceOperation::OpenVideoPlaybackDevice, openedCollectionScope));
    }

    return m_runner->loadVideoPlaybackDevice(videoUrl);
}

void MediaEntrySourceRuntime::startCandidateLoad(MediaEntrySourceCandidateLoadBatch batch)
{
    if (m_runner == nullptr || !m_candidateLoadState.acceptsBatch(batch)) {
        return;
    }

    std::shared_ptr<MediaEntrySourceRunner> runner = m_runner;
    m_candidateLoadStopSource = std::stop_source();
    MediaEntrySourceOpenContext context;
    context.stopToken = m_candidateLoadStopSource.get_token();
    m_candidateLoadTask = m_workerScheduler.run(
        m_context,
        [runner = std::move(runner), context]() {
            return runner->loadImageDocumentPageCandidates(context);
        },
        [this, batch](MediaEntrySourceCandidatesResult result) mutable {
            finishCandidateLoad(batch, std::move(result));
        });
}

void MediaEntrySourceRuntime::finishCandidateLoad(
    MediaEntrySourceCandidateLoadBatch batch, MediaEntrySourceCandidatesResult result)
{
    std::vector<MediaEntrySourceCandidateLoad> pendingLoads
        = m_candidateLoadState.finishBatch(batch);

    for (const MediaEntrySourceCandidateLoad& load : pendingLoads) {
        load.completion.claimAndDelete([&]() {
            finishMediaEntrySourceCandidateResult(result, load.callback, load.errorCallback);
        });
    }
}

void MediaEntrySourceRuntime::cancelCandidateLoadBatch()
{
    m_candidateLoadState.cancel();
    m_candidateLoadStopSource.request_stop();
    m_candidateLoadTask.cancel();
}
}
