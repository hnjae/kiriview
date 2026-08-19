// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADSTATE_H
#define KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADSTATE_H

#include "async/imageasynccallbacks.h"
#include "async/imageasyncoperationstate.h"
#include "async/imageiojob.h"
#include "mediaentrysourcebackend.h"

#include <QtGlobal>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct MediaEntrySourceEntryLoad
{
    ImageIoJobCompletion completion;
    MediaEntrySourceEntriesCallback callback;
    MediaEntrySourceErrorCallback errorCallback;
};

struct MediaEntrySourceEntryLoadBatch
{
    quint64 operationId = 0;
};

class MediaEntrySourceEntryLoadState final
{
public:
    ImageIoJob addLoad(QObject* receiver, MediaEntrySourceEntriesCallback callback,
        MediaEntrySourceErrorCallback errorCallback);
    std::optional<MediaEntrySourceEntryLoadBatch> startBatch();
    [[nodiscard]] bool acceptsBatch(MediaEntrySourceEntryLoadBatch batch) const;
    [[nodiscard]] bool batchInProgress() const;
    std::vector<MediaEntrySourceEntryLoad> finishBatch(MediaEntrySourceEntryLoadBatch batch);
    void cancel();

private:
    void reset();

    std::vector<MediaEntrySourceEntryLoad> m_pendingLoads;
    ImageAsyncOperationState m_batch;
};
}

#endif
