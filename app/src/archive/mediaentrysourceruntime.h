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

    ImageIoJob loadOpenedCollectionCandidates(QObject* receiver,
        OpenedCollectionScopeLocation openedCollectionScope,
        ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback);
    ImageIoJob loadOpenedCollectionImageData(QObject* receiver, const ImageDecodeRequest& request,
        ImageDataCallback callback, MediaEntrySourceErrorCallback errorCallback);
    MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl);

private:
    void startCandidateLoad(MediaEntrySourceCandidateLoadBatch batch);
    void finishCandidateLoad(
        MediaEntrySourceCandidateLoadBatch batch, MediaEntrySourceCandidatesResult result);
    void cancelCandidateLoadBatch();

    QObject* m_context = nullptr;
    MediaEntrySourceFactory m_sourceFactory;
    ImageWorkerScheduler m_workerScheduler;
    std::shared_ptr<ImageSourceDataBudget> m_sourceDataBudget;
    std::shared_ptr<MediaEntrySourceRunner> m_runner;
    MediaEntrySourceCandidateLoadState m_candidateLoadState;
    ImageWorkerTask m_candidateLoadTask;
    ImageAsyncTicket m_sourceGeneration;
};
}

#endif
