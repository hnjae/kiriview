// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidaterepository.h"

#include "async/imagecallback.h"

#include <utility>
#include <vector>

namespace {
void reportLoadProviderMissing(const kiriview::ErrorCallback& errorCallback)
{
    kiriview::invokeIfSet(errorCallback, QString());
}

template <typename Provider, typename... Args>
kiriview::ImageIoJob loadWithProvider(
    const Provider& provider, kiriview::ErrorCallback errorCallback, Args&&... args)
{
    if (!provider) {
        reportLoadProviderMissing(errorCallback);
        return kiriview::ImageIoJob();
    }

    return provider(std::forward<Args>(args)..., std::move(errorCallback));
}

kiriview::ImageIoJob loadImagesForSource(
    const kiriview::ImageDocumentPageCandidateRepository& repository, QObject* receiver,
    const kiriview::ImageDocumentPageCandidateListSource::Directory& source,
    kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    return repository.loadDirectoryImages(receiver, source.directoryUrl, std::move(callback),
        [errorCallback = std::move(errorCallback)](QString error) mutable {
            kiriview::invokeIfSet(
                errorCallback, kiriview::ImageDocumentPageCandidateLoadError { std::move(error) });
        });
}

kiriview::ImageIoJob loadImagesForSource(
    const kiriview::ImageDocumentPageCandidateRepository& repository, QObject* receiver,
    const kiriview::ImageDocumentPageCandidateListSource::OpenedCollectionScope& source,
    kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    return repository.loadOpenedCollectionCandidates(receiver, source.openedCollectionScope,
        std::move(callback),
        [errorCallback = std::move(errorCallback)](kiriview::MediaEntrySourceError error) mutable {
            kiriview::invokeIfSet(
                errorCallback, kiriview::ImageDocumentPageCandidateLoadError { std::move(error) });
        });
}

kiriview::ImageIoJob watchChangesForSource(
    const kiriview::ImageDocumentPageCandidateRepository& repository, QObject* receiver,
    const kiriview::ImageDocumentPageCandidateListSource::Directory& source,
    kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    return repository.watchDirectoryImageChanges(receiver, source.directoryUrl, std::move(callback),
        [errorCallback = std::move(errorCallback)](QString error) mutable {
            kiriview::invokeIfSet(
                errorCallback, kiriview::ImageDocumentPageCandidateLoadError { std::move(error) });
        });
}

kiriview::ImageIoJob watchChangesForSource(const kiriview::ImageDocumentPageCandidateRepository&,
    QObject*, const kiriview::ImageDocumentPageCandidateListSource::OpenedCollectionScope&,
    const kiriview::ImageDocumentPageCandidatesCallback&,
    const kiriview::ImageDocumentPageCandidateLoadErrorCallback&)
{
    return kiriview::ImageIoJob();
}

}

namespace kiriview {
ImageDocumentPageCandidateRepository::ImageDocumentPageCandidateRepository(
    ImageDocumentPageCandidateProvider provider)
    : m_provider(imageDocumentPageNavigationCandidateProviderWithDefaults(std::move(provider)))
{
}

ImageIoJob ImageDocumentPageCandidateRepository::loadImages(QObject* receiver,
    const ImageDocumentPageCandidateListSource& source,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    return source.visit(
        [this, receiver, callback = std::move(callback), errorCallback = std::move(errorCallback)](
            const auto& typedSource) mutable {
            return loadImagesForSource(
                *this, receiver, typedSource, std::move(callback), std::move(errorCallback));
        });
}

ImageIoJob ImageDocumentPageCandidateRepository::loadImages(QObject* receiver,
    const ImageDocumentPageCandidateListContext& context,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    return loadImages(receiver, context.source(), std::move(callback), std::move(errorCallback));
}

ImageIoJob ImageDocumentPageCandidateRepository::loadDirectoryImages(QObject* receiver,
    const QUrl& directoryUrl, ImageDocumentPageCandidatesCallback callback,
    ErrorCallback errorCallback) const
{
    return loadWithProvider(m_provider.directoryImageDocumentPages, std::move(errorCallback),
        receiver, directoryUrl, std::move(callback));
}

ImageIoJob ImageDocumentPageCandidateRepository::loadOpenedCollectionCandidates(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback) const
{
    if (!m_provider.openedCollectionCandidates) {
        invokeIfSet(errorCallback,
            MediaEntrySourceError {
                MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown,
                MediaEntrySourceOperation::ListCandidates,
                openedCollectionScope.fileUrl(),
                {},
                QStringLiteral("opened collection candidate provider is unavailable"),
            });
        return ImageIoJob();
    }

    return m_provider.openedCollectionCandidates(
        receiver, std::move(openedCollectionScope), std::move(callback), std::move(errorCallback));
}

ImageIoJob ImageDocumentPageCandidateRepository::loadContainers(QObject* receiver,
    const QUrl& directoryUrl, ContainerCandidatesCallback callback,
    ErrorCallback errorCallback) const
{
    return loadWithProvider(m_provider.directoryContainers, std::move(errorCallback), receiver,
        directoryUrl, std::move(callback));
}

ImageIoJob ImageDocumentPageCandidateRepository::watchCandidateChanges(QObject* receiver,
    const ImageDocumentPageCandidateListSource& source,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    return source.visit(
        [this, receiver, callback = std::move(callback), errorCallback = std::move(errorCallback)](
            const auto& typedSource) mutable {
            return watchChangesForSource(
                *this, receiver, typedSource, std::move(callback), std::move(errorCallback));
        });
}

ImageIoJob ImageDocumentPageCandidateRepository::watchCandidateChanges(QObject* receiver,
    const ImageDocumentPageCandidateListContext& context,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    return watchCandidateChanges(
        receiver, context.source(), std::move(callback), std::move(errorCallback));
}

ImageIoJob ImageDocumentPageCandidateRepository::watchDirectoryImageChanges(QObject* receiver,
    const QUrl& directoryUrl, ImageDocumentPageCandidatesCallback callback,
    ErrorCallback errorCallback) const
{
    return loadWithProvider(m_provider.directoryImageDocumentPageChanges, std::move(errorCallback),
        receiver, directoryUrl, std::move(callback));
}
}
