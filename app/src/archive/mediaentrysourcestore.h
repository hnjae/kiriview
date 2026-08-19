// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCESTORE_H
#define KIRIVIEW_MEDIAENTRYSOURCESTORE_H

#include "async/imageiojob.h"
#include "decoding/imagedecodedependencies.h"
#include "mediaentrysourcebackend.h"
#include "mediaentrysourceruntime.h"

#include <QObject>

namespace kiriview {
class MediaEntrySourceStore final : public QObject
{
    Q_OBJECT
public:
    explicit MediaEntrySourceStore(MediaEntrySourceFactory sourceFactory = {},
        ImageWorkerScheduler workerScheduler = {},
        std::shared_ptr<ImageSourceDataBudget> sourceDataBudget = {});
    ~MediaEntrySourceStore() override;
    Q_DISABLE_COPY_MOVE(MediaEntrySourceStore)

    ImageDecodeDependencies wrapDecodeDependencies(ImageDecodeDependencies dependencies);

    void prepareForOpenedCollectionScope(
        const OpenedCollectionScopeLocation& openedCollectionScope);
    void clear();

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
    MediaEntrySourceRuntime m_runtime;
};
}

#endif
