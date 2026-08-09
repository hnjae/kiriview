// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidateloading.h"

#include "async/directorylistingjob.h"
#include "async/imagecallback.h"
#include "navigation/imagedocumentpagecandidateitems.h"

#include <utility>

namespace {
kiriview::KioOperationFailure candidateAdmissionFailure(const QUrl& directoryUrl)
{
    return kiriview::kioOperationResourceLimitFailure(kiriview::KioOperationKind::DirectoryListing,
        directoryUrl,
        QStringLiteral("ordinary sibling listing exceeds the configured resource limits"));
}

template <typename CandidateCallback, typename CandidateFactory>
kiriview::ImageIoJob startDirectoryCandidateList(QObject* receiver, const QUrl& directoryUrl,
    CandidateCallback callback, kiriview::KioOperationFailureCallback errorCallback,
    kiriview::DirectoryItemListProvider directoryItemListProvider,
    CandidateFactory candidateFactory)
{
    return kiriview::startDirectoryItemList(
        receiver, directoryUrl,
        [callback = std::move(callback), candidateFactory = std::move(candidateFactory)](
            const KFileItemList& items) mutable {
            kiriview::invokeIfSet(callback, candidateFactory(items));
        },
        std::move(errorCallback), std::move(directoryItemListProvider));
}
}

namespace kiriview {
ImageIoJob startDirectoryImageDocumentPageCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ImageDocumentPageCandidatesCallback callback, KioOperationFailureCallback errorCallback)
{
    return startDirectoryImageDocumentPageCandidateList(
        receiver, directoryUrl, std::move(callback), std::move(errorCallback), {});
}

ImageIoJob startDirectoryImageDocumentPageCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ImageDocumentPageCandidatesCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider directoryItemListProvider)
{
    KioOperationFailureCallback admissionErrorCallback = errorCallback;
    return startDirectoryItemList(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(admissionErrorCallback),
            directoryUrl](const KFileItemList& items) mutable {
            ImageDocumentPageCandidateAdmissionResult admitted
                = imageDocumentPageNavigationCandidates(items);
            if (!admitted) {
                invokeIfSet(errorCallback, candidateAdmissionFailure(directoryUrl));
                return;
            }
            invokeIfSet(callback, std::move(*admitted));
        },
        std::move(errorCallback), std::move(directoryItemListProvider));
}

ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback)
{
    return startDirectoryCandidateList(receiver, directoryUrl, std::move(callback),
        std::move(errorCallback), {}, containerNavigationCandidates);
}

ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider directoryItemListProvider)
{
    return startDirectoryCandidateList(receiver, directoryUrl, std::move(callback),
        std::move(errorCallback), std::move(directoryItemListProvider),
        containerNavigationCandidates);
}
}
