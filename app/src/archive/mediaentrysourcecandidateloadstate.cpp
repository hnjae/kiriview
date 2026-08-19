// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcecandidateloadstate.h"

#include <QObject>
#include <utility>

namespace {
void cancelMediaEntrySourceEntryLoadToken(QObject* object)
{
    if (object != nullptr) {
        object->deleteLater();
    }
}
}

namespace kiriview {
ImageIoJob MediaEntrySourceEntryLoadState::addLoad(QObject* receiver,
    MediaEntrySourceEntriesCallback callback, MediaEntrySourceErrorCallback errorCallback)
{
    QObject* token = new QObject(receiver);
    ImageIoJob job(token, cancelMediaEntrySourceEntryLoadToken);
    m_pendingLoads.push_back(MediaEntrySourceEntryLoad {
        job.completion(),
        std::move(callback),
        std::move(errorCallback),
    });
    return job;
}

std::optional<MediaEntrySourceEntryLoadBatch> MediaEntrySourceEntryLoadState::startBatch()
{
    if (m_batch.active() || m_pendingLoads.empty()) {
        return std::nullopt;
    }

    return MediaEntrySourceEntryLoadBatch {
        m_batch.start(),
    };
}

bool MediaEntrySourceEntryLoadState::acceptsBatch(MediaEntrySourceEntryLoadBatch batch) const
{
    return m_batch.accepts(batch.operationId);
}

bool MediaEntrySourceEntryLoadState::batchInProgress() const { return m_batch.active(); }

std::vector<MediaEntrySourceEntryLoad> MediaEntrySourceEntryLoadState::finishBatch(
    MediaEntrySourceEntryLoadBatch batch)
{
    if (!m_batch.finish(batch.operationId)) {
        return {};
    }

    std::vector<MediaEntrySourceEntryLoad> pendingLoads = std::move(m_pendingLoads);
    reset();
    return pendingLoads;
}

void MediaEntrySourceEntryLoadState::cancel()
{
    for (const MediaEntrySourceEntryLoad& load : m_pendingLoads) {
        load.completion.cancel();
    }
    reset();
}

void MediaEntrySourceEntryLoadState::reset()
{
    m_pendingLoads.clear();
    m_batch.cancel();
}
}
