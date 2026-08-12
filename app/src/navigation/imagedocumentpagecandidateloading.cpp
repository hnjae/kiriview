// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidateloading.h"

#include "async/directorylistingjob.h"
#include "async/imagecallback.h"
#include "navigation/imagedocumentpagecandidateitems.h"

#include <utility>

namespace {
kiriview::KioOperationFailure candidateAdmissionFailure(
    const QUrl& directoryUrl, kiriview::ImageDocumentPageCandidateAdmissionFailure failure)
{
    if (failure == kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded) {
        return kiriview::kioOperationResourceLimitFailure(
            kiriview::KioOperationKind::DirectoryListing, directoryUrl,
            QStringLiteral("ordinary sibling listing exceeds the configured resource limits"));
    }
    return kiriview::kioOperationValidationFailure(kiriview::KioOperationKind::DirectoryListing,
        directoryUrl,
        QStringLiteral(
            "ordinary sibling listing returned a candidate outside the requested scope"));
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
                = imageDocumentPageNavigationCandidates(directoryUrl, items);
            if (!admitted) {
                invokeIfSet(
                    errorCallback, candidateAdmissionFailure(directoryUrl, admitted.error()));
                return;
            }
            invokeIfSet(callback, std::move(*admitted));
        },
        std::move(errorCallback), std::move(directoryItemListProvider));
}

ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback)
{
    return startDirectoryContainerCandidateList(
        receiver, directoryUrl, std::move(callback), std::move(errorCallback), {});
}

ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider directoryItemListProvider)
{
    KioOperationFailureCallback admissionErrorCallback = errorCallback;
    return startDirectoryItemList(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(admissionErrorCallback),
            directoryUrl](const KFileItemList& items) mutable {
            ContainerNavigationCandidateAdmissionResult admitted
                = containerNavigationCandidates(directoryUrl, items);
            if (!admitted) {
                invokeIfSet(
                    errorCallback, candidateAdmissionFailure(directoryUrl, admitted.error()));
                return;
            }
            invokeIfSet(callback, std::move(*admitted));
        },
        std::move(errorCallback), std::move(directoryItemListProvider));
}
}
