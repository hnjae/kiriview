// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "directmedianavigationcandidateprovider.h"

#include "async/directorylistingjob.h"
#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentpagecandidateitems.h"
#include "imagedocumentpagecandidateloaderror.h"
#include "imagedocumentpagecandidatestore.h"
#include "navigationlogging.h"

#include <QDebug>
#include <QObject>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
kiriview::KioOperationFailure directMediaCandidateAdmissionFailure(
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

kiriview::ImageIoJob startDirectoryDirectMediaNavigationCandidateList(QObject* receiver,
    const QUrl& directoryUrl, kiriview::DirectMediaNavigationCandidatesCallback callback,
    kiriview::KioOperationFailureCallback errorCallback,
    kiriview::DirectoryItemListProvider directoryItemListProvider)
{
    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate provider listing directory"
        << "directoryUrl" << kiriview::diagnosticSourceReference(directoryUrl);
    kiriview::KioOperationFailureCallback admissionErrorCallback = errorCallback;
    return kiriview::startDirectoryItemList(
        receiver, directoryUrl,
        [callback = std::move(callback), errorCallback = std::move(admissionErrorCallback),
            directoryUrl](const KFileItemList& items) mutable {
            kiriview::DirectMediaNavigationCandidateAdmissionResult admitted
                = kiriview::directMediaNavigationCandidates(directoryUrl, items);
            if (!admitted) {
                kiriview::invokeIfSet(errorCallback,
                    directMediaCandidateAdmissionFailure(directoryUrl, admitted.error()));
                return;
            }
            std::vector<kiriview::DirectMediaNavigationCandidate> candidates = std::move(*admitted);
            qCDebug(kiriviewNavigationLog)
                << "direct media navigation candidate provider listed directory"
                << "directoryUrl" << kiriview::diagnosticSourceReference(directoryUrl) << "items"
                << items.size() << "candidates" << candidates.size();
            kiriview::invokeIfSet(callback, std::move(candidates));
        },
        [errorCallback = std::move(errorCallback), directoryUrl](
            kiriview::KioOperationFailure failure) mutable {
            qCDebug(kiriviewNavigationLog)
                << "direct media navigation candidate provider listing failed"
                << "directoryUrl" << kiriview::diagnosticSourceReference(directoryUrl) << "error"
                << kiriview::diagnosticDetailReference(failure.diagnosticDetail);
            kiriview::invokeIfSet(errorCallback, std::move(failure));
        },
        std::move(directoryItemListProvider));
}

std::vector<kiriview::DirectMediaNavigationCandidate> directMediaNavigationCandidates(
    std::vector<kiriview::ImageDocumentPageCandidate> pageCandidates)
{
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates;
    candidates.reserve(pageCandidates.size());
    for (kiriview::ImageDocumentPageCandidate& candidate : pageCandidates) {
        candidates.push_back(kiriview::DirectMediaNavigationCandidate {
            std::move(candidate.url), std::move(candidate.name), candidate.sourceFreshness });
    }
    return candidates;
}

kiriview::KioOperationFailure directMediaNavigationCandidateFailure(
    kiriview::ImageDocumentPageCandidateLoadError error, const QUrl& directoryUrl)
{
    return std::visit(
        [&directoryUrl](auto&& detail) -> kiriview::KioOperationFailure {
            using Error = std::decay_t<decltype(detail)>;
            if constexpr (std::is_same_v<Error, kiriview::KioOperationFailure>) {
                return std::forward<decltype(detail)>(detail);
            } else if constexpr (std::is_same_v<Error, QString>) {
                kiriview::KioOperationFailure failure = kiriview::kioOperationValidationFailure(
                    kiriview::KioOperationKind::DirectoryListing, directoryUrl, detail);
                failure.userMessage = detail;
                return failure;
            } else {
                return kiriview::kioOperationValidationFailure(
                    kiriview::KioOperationKind::DirectoryListing, directoryUrl,
                    detail.diagnosticDetail);
            }
        },
        std::move(error));
}

kiriview::ImageIoJob loadDirectMediaNavigationCandidatesFromStore(
    const std::shared_ptr<kiriview::ImageDocumentPageCandidateStore>& store, QObject* receiver,
    const QUrl& directoryUrl, kiriview::DirectMediaNavigationCandidatesCallback callback,
    kiriview::KioOperationFailureCallback errorCallback)
{
    return store->loadDirectoryImages(
        receiver, directoryUrl,
        [callback = std::move(callback)](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) mutable {
            kiriview::invokeIfSet(callback, directMediaNavigationCandidates(std::move(candidates)));
        },
        [errorCallback = std::move(errorCallback), directoryUrl](
            kiriview::ImageDocumentPageCandidateLoadError error) mutable {
            kiriview::invokeIfSet(errorCallback,
                directMediaNavigationCandidateFailure(std::move(error), directoryUrl));
        });
}

kiriview::ImageIoJob watchDirectMediaNavigationCandidatesFromStore(
    const std::shared_ptr<kiriview::ImageDocumentPageCandidateStore>& store, QObject* receiver,
    const QUrl& directoryUrl, kiriview::DirectMediaNavigationCandidatesCallback callback,
    kiriview::KioOperationFailureCallback errorCallback)
{
    return store->watchDirectoryImages(
        receiver, directoryUrl,
        [callback = std::move(callback)](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) mutable {
            kiriview::invokeIfSet(callback, directMediaNavigationCandidates(std::move(candidates)));
        },
        [errorCallback = std::move(errorCallback), directoryUrl](
            kiriview::ImageDocumentPageCandidateLoadError error) mutable {
            kiriview::invokeIfSet(errorCallback,
                directMediaNavigationCandidateFailure(std::move(error), directoryUrl));
        });
}
}

namespace kiriview {
DirectMediaNavigationCandidateProvider defaultDirectMediaNavigationCandidateProvider(
    DirectoryItemListProvider directoryItemListProvider)
{
    if (!directoryItemListProvider) {
        auto store = std::make_shared<ImageDocumentPageCandidateStore>();
        return DirectMediaNavigationCandidateProvider {
            [store](QObject* receiver, const QUrl& directoryUrl,
                DirectMediaNavigationCandidatesCallback callback,
                KioOperationFailureCallback errorCallback) {
                return loadDirectMediaNavigationCandidatesFromStore(
                    store, receiver, directoryUrl, std::move(callback), std::move(errorCallback));
            },
            [store](QObject* receiver, const QUrl& directoryUrl,
                DirectMediaNavigationCandidatesCallback callback,
                KioOperationFailureCallback errorCallback) {
                return watchDirectMediaNavigationCandidatesFromStore(
                    store, receiver, directoryUrl, std::move(callback), std::move(errorCallback));
            },
        };
    }

    return DirectMediaNavigationCandidateProvider {
        [directoryItemListProvider = std::move(directoryItemListProvider)](QObject* receiver,
            const QUrl& directoryUrl, DirectMediaNavigationCandidatesCallback callback,
            KioOperationFailureCallback errorCallback) {
            return startDirectoryDirectMediaNavigationCandidateList(receiver, directoryUrl,
                std::move(callback), std::move(errorCallback), directoryItemListProvider);
        },
        {},
    };
}

DirectMediaNavigationCandidateProvider directMediaNavigationCandidateProviderWithDefault(
    DirectMediaNavigationCandidateProvider provider,
    DirectoryItemListProvider directoryItemListProvider)
{
    if (!provider.directoryCandidateLoader) {
        DirectMediaNavigationCandidateProvider defaults
            = defaultDirectMediaNavigationCandidateProvider(std::move(directoryItemListProvider));
        provider.directoryCandidateLoader = std::move(defaults.directoryCandidateLoader);
        if (!provider.directoryCandidateChanges) {
            provider.directoryCandidateChanges = std::move(defaults.directoryCandidateChanges);
        }
    }
    return provider;
}
}
