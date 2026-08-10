// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedataloading.h"

#include "archive/mediaentrysourcebackend.h"
#include "async/imagecallback.h"
#include "async/imageioworkerjob.h"
#include "decoding/imagesourcedata.h"
#include "location/imagedocumentlocation.h"
#include "system/kiooperationfailure.h"

#include <KIO/Job>
#include <KIO/TransferJob>
#include <KJob>
#include <QObject>
#include <memory>
#include <utility>

namespace {
using kiriview::ImageDataLoadErrorCallback;
using kiriview::MediaEntrySourceImageData;
using kiriview::MediaEntrySourceImageDataResult;

struct StreamingImageSourceData
{
    kiriview::ImageSourceData sourceData;
    bool resourceLimitExceeded = false;
};

template <typename Result, typename SuccessCallback>
void finishMediaEntrySourceWorkerResult(
    Result result, ImageDataLoadErrorCallback errorCallback, SuccessCallback successCallback)
{
    if (!result) {
        kiriview::invokeIfSet(
            errorCallback, kiriview::ImageDataLoadError { std::move(result.error()) });
        return;
    }
    successCallback(std::move(*result));
}

void cancelKJob(QObject* object)
{
    auto* job = qobject_cast<KJob*>(object);
    if (job == nullptr) {
        return;
    }

    job->kill(KJob::Quietly);
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
ImageIoJob startStoredImageDataLoad(QObject* receiver, ImageDecodeRequest request,
    ImageDataCallback callback, ImageDataLoadErrorCallback errorCallback)
{
    return startStoredImageDataLoad(receiver, std::move(request), ImageWorkerScheduler(),
        defaultImageSourceDataBudget(), std::move(callback), std::move(errorCallback));
}

ImageIoJob startStoredImageDataLoad(QObject* receiver, ImageDecodeRequest request,
    const ImageWorkerScheduler& workerScheduler, ImageDataCallback callback,
    ImageDataLoadErrorCallback errorCallback)
{
    return startStoredImageDataLoad(receiver, std::move(request), workerScheduler,
        defaultImageSourceDataBudget(), std::move(callback), std::move(errorCallback));
}

ImageIoJob startStoredImageDataLoad(QObject* receiver, ImageDecodeRequest request,
    const ImageWorkerScheduler& workerScheduler,
    std::shared_ptr<ImageSourceDataBudget> sourceDataBudget, ImageDataCallback callback,
    ImageDataLoadErrorCallback errorCallback)
{
    if (sourceDataBudget == nullptr) {
        sourceDataBudget = defaultImageSourceDataBudget();
    }
    ImageSourceDataLease lease = sourceDataBudget->startLease();
    if (openedCollectionScopeContainsUrl(request.openedCollectionScope(), request.imageUrl())) {
        return startMediaEntrySourceWorkerJob(
            receiver, workerScheduler,
            [request = std::move(request), lease = std::move(lease)]() mutable {
                return loadMediaEntrySourceImageData(
                    request.openedCollectionScope(), request.imageUrl(), std::move(lease));
            },
            [callback = std::move(callback), errorCallback = std::move(errorCallback)](
                MediaEntrySourceImageDataResult result) mutable {
                finishMediaEntrySourceWorkerResult(std::move(result), std::move(errorCallback),
                    [callback = std::move(callback)](MediaEntrySourceImageData data) mutable {
                        kiriview::invokeIfSet(
                            callback, ImageSourceData(std::move(data.data), std::move(data.lease)));
                    });
            });
    }

    auto state = std::make_shared<StreamingImageSourceData>();
    state->sourceData.lease = std::move(lease);
    const QUrl imageUrl = request.imageUrl();
    auto* job = KIO::get(imageUrl, KIO::Reload, KIO::HideProgressInfo);
    ImageIoJob ioJob(job, cancelKJob);
    const ImageIoJobCompletion completion = ioJob.completion();

    QObject::connect(
        job, &KIO::TransferJob::data, receiver, [state, job](KIO::Job*, const QByteArray& chunk) {
            if (state->resourceLimitExceeded || chunk.isEmpty()) {
                return;
            }
            if (state->sourceData.tryAppend(QByteArrayView(chunk))) {
                return;
            }

            state->resourceLimitExceeded = true;
            job->kill(KJob::EmitResult);
        });

    QObject::connect(job, &KJob::result, receiver,
        [completion, imageUrl, state = std::move(state), callback = std::move(callback),
            errorCallback = std::move(errorCallback)](KJob* finishedJob) mutable {
            completion.claimAndRun([&]() {
                if (state->resourceLimitExceeded) {
                    state->sourceData = {};
                    kiriview::invokeIfSet(errorCallback,
                        ImageDataLoadError {
                            kioOperationResourceLimitFailure(KioOperationKind::ImageDataRead,
                                imageUrl, imageSourceDataResourceLimitDiagnostic()) });
                    return;
                }
                if (finishedJob->error() != KJob::NoError) {
                    state->sourceData = {};
                    kiriview::invokeIfSet(errorCallback,
                        ImageDataLoadError {
                            kioOperationFailureFromKJob(KioOperationKind::ImageDataRead, imageUrl,
                                finishedJob->error(), finishedJob->errorString()) });
                    return;
                }

                kiriview::invokeIfSet(callback, std::move(state->sourceData));
            });
        });
    return ioJob;
}
}
