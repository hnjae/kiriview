// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/mediaentrysourcecandidateloading.h"

#include "archive/mediaentrysourcebackend.h"
#include "async/imagecallback.h"
#include "async/imageioworkerjob.h"
#include "localization/mediaentrysourceerrortext.h"
#include "navigation/navigationlogging.h"

#include <utility>

namespace {
using kiriview::ErrorCallback;
using kiriview::MediaEntrySourceCandidates;
using kiriview::MediaEntrySourceCandidatesResult;

template <typename Result, typename SuccessCallback>
void finishMediaEntrySourceWorkerResult(
    Result result, ErrorCallback errorCallback, SuccessCallback successCallback)
{
    if (!result) {
        const kiriview::MediaEntrySourceError& error = result.error();
        qCWarning(kiriviewNavigationLog).noquote()
            << "collection candidate loading failed" << error;
        kiriview::invokeIfSet(errorCallback, kiriview::mediaEntrySourceErrorText(error));
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
ImageIoJob startOpenedCollectionCandidateList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    ImageDocumentPageCandidatesCallback callback, ErrorCallback errorCallback)
{
    return startOpenedCollectionCandidateList(receiver, std::move(openedCollectionScope),
        ImageWorkerScheduler(), std::move(callback), std::move(errorCallback));
}

ImageIoJob startOpenedCollectionCandidateList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    const ImageWorkerScheduler& workerScheduler, ImageDocumentPageCandidatesCallback callback,
    ErrorCallback errorCallback)
{
    return startMediaEntrySourceWorkerJob(
        receiver, workerScheduler,
        [openedCollectionScope = std::move(openedCollectionScope)]() {
            return loadMediaEntrySourceCandidates(openedCollectionScope);
        },
        [callback = std::move(callback), errorCallback = std::move(errorCallback)](
            MediaEntrySourceCandidatesResult result) mutable {
            finishMediaEntrySourceWorkerResult(std::move(result), std::move(errorCallback),
                [callback = std::move(callback)](MediaEntrySourceCandidates candidates) mutable {
                    kiriview::invokeIfSet(callback, std::move(candidates.candidates));
                });
        });
}
}
