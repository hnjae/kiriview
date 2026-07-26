// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCESTORE_H
#define KIRIVIEW_MEDIAENTRYSOURCESTORE_H

#include "async/imageiojob.h"
#include "decoding/imagedecodedependencies.h"
#include "mediaentrysourcebackend.h"
#include "mediaentrysourceruntime.h"
#include "navigation/imagedocumentpagecandidateprovider.h"

#include <QObject>

namespace kiriview {
using MediaEntrySourceErrorProjector = std::function<QString(const MediaEntrySourceError&)>;

class MediaEntrySourceStore final : public QObject
{
    Q_OBJECT
public:
    explicit MediaEntrySourceStore(
        MediaEntrySourceFactory sourceFactory = {}, ImageWorkerScheduler workerScheduler = {});
    ~MediaEntrySourceStore() override;
    Q_DISABLE_COPY_MOVE(MediaEntrySourceStore)

    ImageDocumentPageCandidateProvider wrapCandidateProvider(
        ImageDocumentPageCandidateProvider provider, MediaEntrySourceErrorProjector errorProjector);
    ImageDecodeDependencies wrapDecodeDependencies(
        ImageDecodeDependencies dependencies, MediaEntrySourceErrorProjector errorProjector);

    void prepareForOpenedCollectionScope(
        const OpenedCollectionScopeLocation& openedCollectionScope);
    void clear();

    [[nodiscard]] bool hasCurrentOpenedCollectionScope() const;
    [[nodiscard]] bool hasCurrentOpenedCollectionScope(
        const OpenedCollectionScopeLocation& openedCollectionScope) const;

    ImageIoJob loadOpenedCollectionCandidates(QObject* receiver,
        OpenedCollectionScopeLocation openedCollectionScope,
        ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback);
    ImageIoJob loadOpenedCollectionImageData(QObject* receiver, const ImageDecodeRequest& request,
        ImageDataCallback callback, MediaEntrySourceErrorCallback errorCallback);
    MediaEntrySourceVideoPlaybackDeviceResult loadOpenedCollectionVideoPlaybackDevice(
        OpenedCollectionScopeLocation openedCollectionScope, const QUrl& videoUrl);

private:
    MediaEntrySourceRuntime m_runtime;
};
}

#endif
