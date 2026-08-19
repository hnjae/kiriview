// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidaterepository.h"

#include "async/imagecallback.h"
#include "imagedocumentpagecandidateitems.h"

#include <utility>
#include <vector>

namespace {
kiriview::KioOperationFailure candidateScopeFailure(const QUrl& directoryUrl)
{
    return kiriview::kioOperationValidationFailure(kiriview::KioOperationKind::DirectoryListing,
        directoryUrl,
        QStringLiteral(
            "directory candidate provider returned a candidate outside the requested scope"));
}

void reportLoadProviderMissing(
    const kiriview::ImageDocumentPageCandidateLoadErrorCallback& errorCallback)
{
    kiriview::invokeIfSet(
        errorCallback, kiriview::ImageDocumentPageCandidateLoadError { QString() });
}

kiriview::ImageIoJob loadImagesForSource(
    const kiriview::ImageDocumentPageCandidateRepository& repository, QObject* receiver,
    const kiriview::ImageDocumentPageCandidateListSource::Directory& source,
    kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    return repository.loadDirectoryImages(
        receiver, source.directoryUrl, std::move(callback), std::move(errorCallback));
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
    return repository.watchDirectoryImageChanges(
        receiver, source.directoryUrl, std::move(callback), std::move(errorCallback));
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
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    if (!m_provider.directoryImageDocumentPages) {
        reportLoadProviderMissing(errorCallback);
        return ImageIoJob();
    }

    ImageDocumentPageCandidateLoadErrorCallback scopeErrorCallback = errorCallback;
    return m_provider.directoryImageDocumentPages(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(scopeErrorCallback),
            directoryUrl](std::vector<ImageDocumentPageCandidate> candidates) mutable {
            if (!imageDocumentPageCandidatesBelongToDirectoryScope(candidates, directoryUrl)) {
                invokeIfSet(errorCallback,
                    ImageDocumentPageCandidateLoadError { candidateScopeFailure(directoryUrl) });
                return;
            }
            invokeIfSet(callback, std::move(candidates));
        },
        std::move(errorCallback));
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
                MediaEntrySourceOperation::ListEntries,
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
    KioOperationFailureCallback errorCallback) const
{
    if (!m_provider.directoryContainers) {
        invokeIfSet(errorCallback,
            kioOperationValidationFailure(KioOperationKind::DirectoryListing, directoryUrl,
                QStringLiteral("directory container candidate provider is unavailable")));
        return ImageIoJob();
    }

    KioOperationFailureCallback scopeErrorCallback = errorCallback;
    return m_provider.directoryContainers(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(scopeErrorCallback),
            directoryUrl](std::vector<ContainerNavigationCandidate> candidates) mutable {
            if (!containerNavigationCandidatesBelongToDirectoryScope(candidates, directoryUrl)) {
                invokeIfSet(errorCallback, candidateScopeFailure(directoryUrl));
                return;
            }
            invokeIfSet(callback, std::move(candidates));
        },
        std::move(errorCallback));
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
    ImageDocumentPageCandidateLoadErrorCallback errorCallback) const
{
    if (!m_provider.directoryImageDocumentPageChanges) {
        reportLoadProviderMissing(errorCallback);
        return ImageIoJob();
    }

    ImageDocumentPageCandidateLoadErrorCallback scopeErrorCallback = errorCallback;
    return m_provider.directoryImageDocumentPageChanges(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(scopeErrorCallback),
            directoryUrl](std::vector<ImageDocumentPageCandidate> candidates) mutable {
            if (!imageDocumentPageCandidatesBelongToDirectoryScope(candidates, directoryUrl)) {
                invokeIfSet(errorCallback,
                    ImageDocumentPageCandidateLoadError { candidateScopeFailure(directoryUrl) });
                return;
            }
            invokeIfSet(callback, std::move(candidates));
        },
        std::move(errorCallback));
}
}
