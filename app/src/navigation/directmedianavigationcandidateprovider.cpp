// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "directmedianavigationcandidateprovider.h"

#include "async/directorylistingjob.h"
#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentpagecandidateitems.h"
#include "navigationlogging.h"

#include <QDebug>
#include <QObject>
#include <utility>
#include <vector>

namespace {
kiriview::ImageIoJob startDirectoryDirectMediaNavigationCandidateList(QObject* receiver,
    const QUrl& directoryUrl, kiriview::DirectMediaNavigationCandidatesCallback callback,
    kiriview::KioOperationFailureCallback errorCallback,
    kiriview::DirectoryItemListProvider directoryItemListProvider)
{
    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate provider listing directory"
        << "directoryUrl" << kiriview::diagnosticSourceReference(directoryUrl);
    return kiriview::startDirectoryItemList(
        receiver, directoryUrl,
        [callback = std::move(callback), directoryUrl](const KFileItemList& items) mutable {
            std::vector<kiriview::DirectMediaNavigationCandidate> candidates
                = kiriview::directMediaNavigationCandidates(items);
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
}

namespace kiriview {
DirectMediaNavigationCandidateProvider defaultDirectMediaNavigationCandidateProvider(
    DirectoryItemListProvider directoryItemListProvider)
{
    return DirectMediaNavigationCandidateProvider {
        [directoryItemListProvider = std::move(directoryItemListProvider)](QObject* receiver,
            const QUrl& directoryUrl, DirectMediaNavigationCandidatesCallback callback,
            KioOperationFailureCallback errorCallback) {
            return startDirectoryDirectMediaNavigationCandidateList(receiver, directoryUrl,
                std::move(callback), std::move(errorCallback), directoryItemListProvider);
        },
    };
}

DirectMediaNavigationCandidateProvider directMediaNavigationCandidateProviderWithDefault(
    DirectMediaNavigationCandidateProvider provider,
    DirectoryItemListProvider directoryItemListProvider)
{
    if (!provider.directoryCandidateLoader) {
        provider
            = defaultDirectMediaNavigationCandidateProvider(std::move(directoryItemListProvider));
    }
    return provider;
}
}
