// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/mediaentrysourcecandidateloading.h"

#include "archive/mediaentrysourcebackend.h"
#include "async/imagecallback.h"
#include "async/imageioworkerjob.h"

#include <utility>

namespace {
using kiriview::MediaEntrySourceEntries;
using kiriview::MediaEntrySourceEntriesResult;
using kiriview::MediaEntrySourceErrorCallback;

template <typename Result, typename SuccessCallback>
void finishMediaEntrySourceWorkerResult(
    Result result, MediaEntrySourceErrorCallback errorCallback, SuccessCallback successCallback)
{
    if (!result) {
        kiriview::invokeIfSet(errorCallback, std::move(result.error()));
        return;
    }
    successCallback(std::move(*result));
}

template <typename Work, typename Finish>
kiriview::ImageIoJob startMediaEntrySourceWorkerJob(QObject* receiver,
    const kiriview::ImageWorkerScheduler& workerScheduler, Work work, Finish finish)
{
    return kiriview::startImageIoWorkerJob(
        receiver, workerScheduler, std::move(work), std::move(finish));
}
}

namespace kiriview {
ImageIoJob startOpenedCollectionEntryList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceEntriesCallback callback,
    MediaEntrySourceErrorCallback errorCallback)
{
    return startOpenedCollectionEntryList(receiver, std::move(openedCollectionScope),
        ImageWorkerScheduler(), std::move(callback), std::move(errorCallback));
}

ImageIoJob startOpenedCollectionEntryList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    const ImageWorkerScheduler& workerScheduler, MediaEntrySourceEntriesCallback callback,
    MediaEntrySourceErrorCallback errorCallback)
{
    return startMediaEntrySourceWorkerJob(
        receiver, workerScheduler,
        [openedCollectionScope = std::move(openedCollectionScope)]() {
            return loadMediaEntrySourceEntries(openedCollectionScope);
        },
        [callback = std::move(callback), errorCallback = std::move(errorCallback)](
            MediaEntrySourceEntriesResult result) mutable {
            finishMediaEntrySourceWorkerResult(std::move(result), std::move(errorCallback),
                [callback = std::move(callback)](MediaEntrySourceEntries entries) mutable {
                    kiriview::invokeIfSet(callback, std::move(entries.entries));
                });
        });
}
}
