// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidateprovider.h"

#include "archive/mediaentrysourcecandidateloading.h"
#include "async/imagecallback.h"
#include "imagedocumentpagecandidateloading.h"
#include "imagedocumentpagecandidatestore.h"
#include "imagedocumentpagenavigationpolicy.h"

#include <memory>
#include <utility>

namespace {
kiriview::ImageIoJob noOpImageDocumentPageCandidateChanges(QObject*, const QUrl&,
    const kiriview::ImageDocumentPageCandidatesCallback&,
    const kiriview::ImageDocumentPageCandidateLoadErrorCallback&)
{
    return kiriview::ImageIoJob();
}

std::vector<kiriview::ImageDocumentPageCandidate> imageDocumentPageCandidatesForEntries(
    std::vector<kiriview::MediaEntrySourceEntry> entries)
{
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    candidates.reserve(entries.size());
    for (kiriview::MediaEntrySourceEntry& entry : entries) {
        candidates.push_back(kiriview::ImageDocumentPageCandidate {
            std::move(entry.url),
            std::move(entry.name),
            entry.kind == kiriview::MediaEntrySourceEntryKind::Video
                ? kiriview::ImageDocumentPageKind::Video
                : kiriview::ImageDocumentPageKind::Image,
        });
    }
    kiriview::sortImageDocumentPageCandidates(&candidates);
    return candidates;
}
}

namespace kiriview {
ImageDocumentPageCandidateProvider defaultImageDocumentPageCandidateProvider(
    ImageWorkerScheduler workerScheduler, DirectoryItemListProvider directoryItemListProvider)
{
    auto candidateStore = std::make_shared<ImageDocumentPageCandidateStore>();
    ImageDocumentPageCandidateProvider provider {
        [candidateStore](QObject* receiver, QUrl directoryUrl,
            ImageDocumentPageCandidatesCallback callback,
            ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
            return candidateStore->loadDirectoryImages(
                receiver, std::move(directoryUrl), std::move(callback), std::move(errorCallback));
        },
        [directoryItemListProvider = std::move(directoryItemListProvider)](QObject* receiver,
            const QUrl& directoryUrl, ContainerCandidatesCallback callback,
            KioOperationFailureCallback errorCallback) {
            return startDirectoryContainerCandidateList(receiver, directoryUrl, std::move(callback),
                std::move(errorCallback), directoryItemListProvider);
        },
        {},
        [candidateStore](QObject* receiver, QUrl directoryUrl,
            ImageDocumentPageCandidatesCallback callback,
            ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
            return candidateStore->watchDirectoryImages(
                receiver, std::move(directoryUrl), std::move(callback), std::move(errorCallback));
        },
    };
    return imageDocumentPageCandidateProviderWithOpenedCollectionEntryLoader(std::move(provider),
        [workerScheduler = std::move(workerScheduler)](QObject* receiver,
            OpenedCollectionScopeLocation openedCollectionScope,
            MediaEntrySourceEntriesCallback callback, MediaEntrySourceErrorCallback errorCallback) {
            return startOpenedCollectionEntryList(receiver, std::move(openedCollectionScope),
                workerScheduler, std::move(callback), std::move(errorCallback));
        });
}

ImageDocumentPageCandidateProvider imageDocumentPageNavigationCandidateProviderWithDefaults(
    ImageDocumentPageCandidateProvider provider, ImageWorkerScheduler workerScheduler,
    DirectoryItemListProvider directoryItemListProvider)
{
    const bool providerIsEmpty = !provider.directoryImageDocumentPages
        && !provider.directoryContainers && !provider.openedCollectionCandidates
        && !provider.directoryImageDocumentPageChanges;
    ImageDocumentPageCandidateProvider defaults = defaultImageDocumentPageCandidateProvider(
        std::move(workerScheduler), std::move(directoryItemListProvider));
    if (!provider.directoryImageDocumentPages) {
        provider.directoryImageDocumentPages = std::move(defaults.directoryImageDocumentPages);
    }
    if (!provider.directoryContainers) {
        provider.directoryContainers = std::move(defaults.directoryContainers);
    }
    if (!provider.openedCollectionCandidates) {
        provider.openedCollectionCandidates = std::move(defaults.openedCollectionCandidates);
    }
    if (!provider.directoryImageDocumentPageChanges) {
        provider.directoryImageDocumentPageChanges = providerIsEmpty
            ? std::move(defaults.directoryImageDocumentPageChanges)
            : noOpImageDocumentPageCandidateChanges;
    }

    return provider;
}

ImageDocumentPageCandidateProvider
imageDocumentPageCandidateProviderWithOpenedCollectionEntryLoader(
    ImageDocumentPageCandidateProvider provider, MediaEntrySourceEntryLoader entryLoader)
{
    provider.openedCollectionCandidates = [entryLoader = std::move(entryLoader)](QObject* receiver,
                                              OpenedCollectionScopeLocation openedCollectionScope,
                                              ImageDocumentPageCandidatesCallback callback,
                                              MediaEntrySourceErrorCallback errorCallback) {
        return entryLoader(
            receiver, std::move(openedCollectionScope),
            [callback = std::move(callback)](std::vector<MediaEntrySourceEntry> entries) mutable {
                invokeIfSet(callback, imageDocumentPageCandidatesForEntries(std::move(entries)));
            },
            std::move(errorCallback));
    };
    return provider;
}
}
