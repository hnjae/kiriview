// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcestore.h"

#include "async/imagecallback.h"
#include "location/imagedocumentlocation.h"

#include <utility>

namespace kiriview {
MediaEntrySourceStore::MediaEntrySourceStore(
    MediaEntrySourceFactory sourceFactory, ImageWorkerScheduler workerScheduler)
    : m_runtime(this, std::move(sourceFactory), std::move(workerScheduler))
{
}

MediaEntrySourceStore::~MediaEntrySourceStore() { clear(); }

ImageDocumentPageCandidateProvider MediaEntrySourceStore::wrapCandidateProvider(
    ImageDocumentPageCandidateProvider provider, MediaEntrySourceErrorProjector errorProjector)
{
    if (!errorProjector) {
        qFatal("MediaEntrySourceStore::wrapCandidateProvider requires an error projector");
    }

    provider.openedCollectionCandidates
        = [this, errorProjector = std::move(errorProjector)](QObject* receiver,
              OpenedCollectionScopeLocation openedCollectionScope,
              ImageDocumentPageCandidatesCallback callback, ErrorCallback errorCallback) {
              return loadOpenedCollectionCandidates(receiver, std::move(openedCollectionScope),
                  std::move(callback),
                  [errorProjector, errorCallback = std::move(errorCallback)](
                      const MediaEntrySourceError& error) {
                      invokeIfSet(errorCallback, errorProjector(error));
                  });
          };
    return provider;
}

ImageDecodeDependencies MediaEntrySourceStore::wrapDecodeDependencies(
    ImageDecodeDependencies dependencies, MediaEntrySourceErrorProjector errorProjector)
{
    if (!errorProjector) {
        qFatal("MediaEntrySourceStore::wrapDecodeDependencies requires an error projector");
    }

    ImageDataLoader upstreamDataLoader = std::move(dependencies.dataLoader);
    dependencies.dataLoader = [this, upstreamDataLoader = std::move(upstreamDataLoader),
                                  errorProjector = std::move(errorProjector)](QObject* receiver,
                                  ImageDecodeRequest request, ImageDataCallback callback,
                                  ErrorCallback errorCallback) {
        if (openedCollectionScopeContainsUrl(request.openedCollectionScope(), request.imageUrl())) {
            return loadOpenedCollectionImageData(receiver, request, std::move(callback),
                [errorProjector, errorCallback = std::move(errorCallback)](
                    const MediaEntrySourceError& error) {
                    invokeIfSet(errorCallback, errorProjector(error));
                });
        }

        if (!upstreamDataLoader) {
            invokeIfSet(errorCallback, QString());
            return ImageIoJob();
        }

        return upstreamDataLoader(
            receiver, std::move(request), std::move(callback), std::move(errorCallback));
    };
    return dependencies;
}

void MediaEntrySourceStore::prepareForOpenedCollectionScope(
    const OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (!openedCollectionScope.isEmpty()) {
        m_runtime.switchToOpenedCollectionScope(openedCollectionScope);
        return;
    }

    clear();
}

void MediaEntrySourceStore::clear() { m_runtime.clear(); }

bool MediaEntrySourceStore::hasCurrentOpenedCollectionScope() const
{
    return m_runtime.hasCurrentOpenedCollectionScope();
}

bool MediaEntrySourceStore::hasCurrentOpenedCollectionScope(
    const OpenedCollectionScopeLocation& openedCollectionScope) const
{
    return m_runtime.hasCurrentOpenedCollectionScope(openedCollectionScope);
}

ImageIoJob MediaEntrySourceStore::loadOpenedCollectionCandidates(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback)
{
    return m_runtime.loadOpenedCollectionCandidates(
        receiver, std::move(openedCollectionScope), std::move(callback), std::move(errorCallback));
}

ImageIoJob MediaEntrySourceStore::loadOpenedCollectionImageData(QObject* receiver,
    const ImageDecodeRequest& request, ImageDataCallback callback,
    MediaEntrySourceErrorCallback errorCallback)
{
    return m_runtime.loadOpenedCollectionImageData(
        receiver, request, std::move(callback), std::move(errorCallback));
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceStore::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl)
{
    return m_runtime.loadOpenedCollectionVideoPlaybackDevice(openedCollectionScope, videoUrl);
}
}
