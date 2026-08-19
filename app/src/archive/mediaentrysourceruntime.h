// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCERUNTIME_H
#define KIRIVIEW_MEDIAENTRYSOURCERUNTIME_H

#include "async/imageasyncticket.h"
#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "decoding/imagedecodedependencies.h"
#include "mediaentrysourcebackend.h"
#include "mediaentrysourcecandidateloadstate.h"

#include <memory>
#include <stop_token>

class QObject;

namespace kiriview {
class MediaEntrySourceRunner;

class MediaEntrySourceRuntime final
{
public:
    explicit MediaEntrySourceRuntime(QObject* context, MediaEntrySourceFactory sourceFactory = {},
        ImageWorkerScheduler workerScheduler = {},
        std::shared_ptr<ImageSourceDataBudget> sourceDataBudget = {});
    ~MediaEntrySourceRuntime();
    Q_DISABLE_COPY_MOVE(MediaEntrySourceRuntime)

    void clear();
    void switchToOpenedCollectionScope(OpenedCollectionScopeLocation openedCollectionScope);

    [[nodiscard]] bool hasCurrentOpenedCollectionScope() const;
    [[nodiscard]] bool hasCurrentOpenedCollectionScope(
        const OpenedCollectionScopeLocation& openedCollectionScope) const;

    ImageIoJob loadOpenedCollectionEntries(QObject* receiver,
        OpenedCollectionScopeLocation openedCollectionScope,
        MediaEntrySourceEntriesCallback callback, MediaEntrySourceErrorCallback errorCallback);
    ImageIoJob loadOpenedCollectionImageData(QObject* receiver, const ImageDecodeRequest& request,
        ImageDataCallback callback, MediaEntrySourceErrorCallback errorCallback);
    MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl);

private:
    void startEntryLoad(MediaEntrySourceEntryLoadBatch batch);
    void finishEntryLoad(MediaEntrySourceEntryLoadBatch batch, quint64 sourceGeneration,
        MediaEntrySourceEntriesResult result);
    void cancelEntryLoadBatch();

    QObject* m_context = nullptr;
    MediaEntrySourceFactory m_sourceFactory;
    ImageWorkerScheduler m_workerScheduler;
    std::shared_ptr<ImageSourceDataBudget> m_sourceDataBudget;
    std::shared_ptr<MediaEntrySourceRunner> m_runner;
    MediaEntrySourceEntryLoadState m_entryLoadState;
    ImageWorkerTask m_entryLoadTask;
    std::stop_source m_entryLoadStopSource;
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    std::shared_ptr<ImageAsyncTicket> m_sourceGeneration = std::make_shared<ImageAsyncTicket>();
};
}

#endif
