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

void finishMediaEntrySourceEntriesResult(const kiriview::MediaEntrySourceEntriesResult& result,
    const kiriview::MediaEntrySourceEntriesCallback& callback,
    const kiriview::MediaEntrySourceErrorCallback& errorCallback)
{
    if (const auto* error = kiriview::mediaEntrySourceResultError(result)) {
        kiriview::invokeIfSet(errorCallback, *error);
        return;
    }

    const auto* entries = kiriview::mediaEntrySourceResultValue(result);
    if (entries != nullptr) {
        kiriview::invokeIfSet(callback, entries->entries);
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

MediaEntrySourceRuntime::~MediaEntrySourceRuntime()
{
    m_callbackLifetime.reset();
    clear();
}

void MediaEntrySourceRuntime::clear()
{
    cancelEntryLoadBatch();
    m_sourceGeneration->invalidate();
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

    cancelEntryLoadBatch();
    m_sourceGeneration->invalidate();
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

ImageIoJob MediaEntrySourceRuntime::loadOpenedCollectionEntries(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceEntriesCallback callback,
    MediaEntrySourceErrorCallback errorCallback)
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

    if (receiver != nullptr && m_entryLoadState.batchInProgress()) {
        return m_entryLoadState.addLoad(receiver, std::move(callback), std::move(errorCallback));
    }

    if (const std::optional<std::vector<MediaEntrySourceEntry>> cachedEntries
        = m_runner->cachedEntries()) {
        invokeIfSet(callback, *cachedEntries);
        return ImageIoJob();
    }

    if (receiver == nullptr) {
        finishMediaEntrySourceEntriesResult(m_runner->loadEntries(), callback, errorCallback);
        return ImageIoJob();
    }

    ImageIoJob ioJob
        = m_entryLoadState.addLoad(receiver, std::move(callback), std::move(errorCallback));
    if (const std::optional<MediaEntrySourceEntryLoadBatch> batch = m_entryLoadState.startBatch()) {
        startEntryLoad(*batch);
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

    const quint64 generation = m_sourceGeneration->current();
    const std::shared_ptr<ImageAsyncTicket> sourceGeneration = m_sourceGeneration;
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const QUrl& imageUrl = request.imageUrl();
    std::shared_ptr<MediaEntrySourceRunner> runner = m_runner;
    ImageSourceDataLease lease = m_sourceDataBudget->startLease();

    return startImageIoWorkerJob(
        m_context, receiver, m_workerScheduler,
        [runner = std::move(runner), imageUrl, lease = std::move(lease)]() mutable {
            return runner->loadImageData(imageUrl, std::move(lease));
        },
        [generation, sourceGeneration, lifetime, callback = std::move(callback),
            errorCallback = std::move(errorCallback)](
            MediaEntrySourceImageDataResult result) mutable {
            if (lifetime.expired() || !sourceGeneration->accepts(generation)) {
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

void MediaEntrySourceRuntime::startEntryLoad(MediaEntrySourceEntryLoadBatch batch)
{
    if (m_runner == nullptr || !m_entryLoadState.acceptsBatch(batch)) {
        return;
    }

    std::shared_ptr<MediaEntrySourceRunner> runner = m_runner;
    const quint64 sourceGenerationValue = m_sourceGeneration->current();
    const std::shared_ptr<ImageAsyncTicket> sourceGeneration = m_sourceGeneration;
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    m_entryLoadStopSource = std::stop_source();
    MediaEntrySourceOpenContext context;
    context.stopToken = m_entryLoadStopSource.get_token();
    ImageWorkerTask task = m_workerScheduler.run(
        m_context, [runner = std::move(runner), context]() { return runner->loadEntries(context); },
        [this, batch, sourceGenerationValue, sourceGeneration, lifetime](
            MediaEntrySourceEntriesResult result) mutable {
            if (lifetime.expired() || !sourceGeneration->accepts(sourceGenerationValue)) {
                return;
            }
            finishEntryLoad(batch, sourceGenerationValue, std::move(result));
        });

    if (lifetime.expired() || !sourceGeneration->accepts(sourceGenerationValue)) {
        task.cancel();
        return;
    }
    if (!m_entryLoadState.acceptsBatch(batch)) {
        task.cancel();
        return;
    }
    m_entryLoadTask = std::move(task);
}

void MediaEntrySourceRuntime::finishEntryLoad(MediaEntrySourceEntryLoadBatch batch,
    quint64 sourceGenerationValue, MediaEntrySourceEntriesResult result)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const std::shared_ptr<ImageAsyncTicket> sourceGeneration = m_sourceGeneration;
    std::vector<MediaEntrySourceEntryLoad> pendingLoads = m_entryLoadState.finishBatch(batch);

    for (const MediaEntrySourceEntryLoad& load : pendingLoads) {
        if (lifetime.expired() || !sourceGeneration->accepts(sourceGenerationValue)) {
            load.completion.cancel();
            continue;
        }
        load.completion.claimAndDelete([&]() {
            finishMediaEntrySourceEntriesResult(result, load.callback, load.errorCallback);
        });
    }
}

void MediaEntrySourceRuntime::cancelEntryLoadBatch()
{
    m_entryLoadState.cancel();
    m_entryLoadStopSource.request_stop();
    m_entryLoadTask.cancel();
}
}
