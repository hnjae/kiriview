// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/videothumbnailextractionadapter.h"

#include <QObject>
#include <utility>

namespace {
class VideoThumbnailExtractionAdapter final : public QObject
{
public:
    using QObject::QObject;

    void start(kiriview::VideoThumbnailExtractionRequest request,
        kiriview::VideoThumbnailExtractionCallback callback,
        kiriview::ImageIoJobCompletion completion)
    {
        m_job = kiriview::startVideoThumbnailExtraction(this, std::move(request),
            [callback = std::move(callback), completion = std::move(completion)](
                kiriview::VideoThumbnailExtractionResult result) mutable {
                QObject* object = completion.object();
                completion.claimAndRun(
                    [object, callback = std::move(callback), result = std::move(result)]() mutable {
                        object->deleteLater();
                        callback(std::move(result));
                    });
            });
    }

    void cancel() { m_job.cancel(); }

private:
    kiriview::VideoThumbnailExtractionJob m_job;
};
}

namespace kiriview {
ImageIoJob startThumbnailVideoExtractionJob(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback callback)
{
    if (receiver == nullptr || !callback) {
        return {};
    }

    auto* adapter = new VideoThumbnailExtractionAdapter(receiver);
    ImageIoJob job(adapter, [](QObject* object) {
        if (object == nullptr) {
            return;
        }
        auto* adapter = static_cast<VideoThumbnailExtractionAdapter*>(object);
        adapter->cancel();
        adapter->deleteLater();
    });
    adapter->start(std::move(request), std::move(callback), job.completion());
    return job;
}
}
