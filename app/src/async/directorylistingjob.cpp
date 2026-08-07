// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/directorylistingjob.h"

#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "system/kiooperationfailure.h"

#include <KCoreDirLister>
#include <KIO/Job>
#include <KJob>
#include <QDebug>
#include <QObject>
#include <utility>

namespace {
void cancelDirLister(QObject* object)
{
    auto* lister = qobject_cast<KCoreDirLister*>(object);
    if (lister == nullptr) {
        return;
    }

    lister->stop();
    lister->deleteLater();
}

KCoreDirLister* createDirectoryItemLister(QObject* parent)
{
    auto* lister = new KCoreDirLister(parent);
    lister->setAutoErrorHandlingEnabled(false);
    lister->setAutoUpdate(false);
    lister->setDelayedMimeTypes(true);
    lister->setShowHiddenFiles(true);
    return lister;
}

void finishDirectoryItemListWithError(const kiriview::ImageIoJobCompletion& completion,
    kiriview::KioOperationFailure failure,
    const kiriview::KioOperationFailureCallback& errorCallback)
{
    completion.claimAndDelete([&]() { kiriview::invokeIfSet(errorCallback, std::move(failure)); });
}

void warnDirectoryListingRejectedEmptyUrl()
{
    qWarning().noquote() << QStringLiteral("KiriView directory listing rejected empty URL");
}

void warnDirectoryListingOpenFailure(const QUrl& directoryUrl)
{
    qWarning().noquote() << "KiriView directory listing openUrl failed"
                         << kiriview::diagnosticSourceReference(directoryUrl);
}

void warnDirectoryListingJobFailure(const QUrl& directoryUrl, const QString& errorString)
{
    qWarning().noquote() << "KiriView directory listing job failed"
                         << kiriview::diagnosticSourceReference(directoryUrl)
                         << kiriview::diagnosticDetailReference(errorString);
}
}

namespace kiriview {
namespace {
    ImageIoJob startKCoreDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
        DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback)
    {
        auto* lister = createDirectoryItemLister(receiver);
        ImageIoJob ioJob(lister, cancelDirLister);
        const ImageIoJobCompletion completion = ioJob.completion();

        QObject::connect(lister, &KCoreDirLister::completed, receiver,
            [completion, lister, callback = std::move(callback)]() mutable {
                completion.claimAndDelete([&]() {
                    kiriview::invokeIfSet(callback, lister->items(KCoreDirLister::AllItems));
                });
            });
        QObject::connect(lister, &KCoreDirLister::jobError, receiver,
            [completion, directoryUrl, errorCallback](KIO::Job* job) {
                KioOperationFailure failure = job == nullptr
                    ? kioOperationValidationFailure(KioOperationKind::DirectoryListing,
                          directoryUrl, QStringLiteral("directory listing emitted a null job"))
                    : kioOperationFailureFromKJob(KioOperationKind::DirectoryListing, directoryUrl,
                          job->error(), job->errorString());
                warnDirectoryListingJobFailure(directoryUrl, failure.diagnosticDetail);
                finishDirectoryItemListWithError(completion, std::move(failure), errorCallback);
            });

        if (directoryUrl.isEmpty()) {
            warnDirectoryListingRejectedEmptyUrl();
            finishDirectoryItemListWithError(completion,
                kioOperationValidationFailure(KioOperationKind::DirectoryListing, directoryUrl,
                    QStringLiteral("empty directory URL")),
                errorCallback);
            return ioJob;
        }

        if (!lister->openUrl(directoryUrl, KCoreDirLister::Reload)) {
            warnDirectoryListingOpenFailure(directoryUrl);
            finishDirectoryItemListWithError(completion,
                kioOperationValidationFailure(KioOperationKind::DirectoryListing, directoryUrl,
                    QStringLiteral("directory listing URL was rejected")),
                errorCallback);
        }

        return ioJob;
    }
}

ImageIoJob startDirectoryItemList(QObject* receiver, QUrl directoryUrl,
    DirectoryItemListCallback callback, KioOperationFailureCallback errorCallback)
{
    return startDirectoryItemList(receiver, std::move(directoryUrl), std::move(callback),
        std::move(errorCallback), defaultDirectoryItemListProvider());
}

ImageIoJob startDirectoryItemList(QObject* receiver, QUrl directoryUrl,
    DirectoryItemListCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider provider)
{
    if (!provider) {
        provider = defaultDirectoryItemListProvider();
    }
    return provider(
        receiver, std::move(directoryUrl), std::move(callback), std::move(errorCallback));
}

DirectoryItemListProvider defaultDirectoryItemListProvider()
{
    return [](QObject* receiver, const QUrl& directoryUrl, DirectoryItemListCallback callback,
               const KioOperationFailureCallback& errorCallback) {
        return startKCoreDirectoryItemList(
            receiver, directoryUrl, std::move(callback), errorCallback);
    };
}
}
