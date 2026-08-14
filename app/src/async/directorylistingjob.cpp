// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/directorylistingjob.h"

#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "system/kiooperationfailure.h"

#include <KFileItem>
#include <KIO/Job>
#include <KIO/ListJob>
#include <KIO/UDSEntry>
#include <KJob>
#include <QDebug>
#include <QObject>
#include <QPointer>
#include <memory>
#include <utility>

namespace {
constexpr qsizetype defaultMaximumSiblingEntryCount = 65'536;
constexpr qsizetype defaultMaximumSiblingIdentityCodeUnitCount = qsizetype { 8 } * 1024 * 1024;

void cancelDirectoryItemList(QObject* object)
{
    auto* job = qobject_cast<KIO::ListJob*>(object);
    if (job == nullptr) {
        return;
    }

    job->kill(KJob::Quietly);
    job->deleteLater();
}

void warnDirectoryListingRejectedEmptyUrl()
{
    qWarning().noquote() << QStringLiteral("KiriView directory listing rejected empty URL");
}

void warnDirectoryListingJobFailure(const QUrl& directoryUrl, const QString& errorString)
{
    qWarning().noquote() << "KiriView directory listing job failed"
                         << kiriview::diagnosticSourceReference(directoryUrl)
                         << kiriview::diagnosticDetailReference(errorString);
}

kiriview::DirectoryItem directoryItem(const KIO::UDSEntry& entry, const QUrl& directoryUrl)
{
    const KFileItem item(entry, directoryUrl, true, true);
    kiriview::DirectoryItem result {
        item.url(),
        item.name(),
        item.isFile(),
    };
    if (entry.contains(KIO::UDSEntry::UDS_SIZE)) {
        result.byteSize = entry.numberValue(KIO::UDSEntry::UDS_SIZE);
    }
    if (entry.contains(KIO::UDSEntry::UDS_MODIFICATION_TIME)) {
        result.modificationTimeSeconds = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME);
    }
    return result;
}

bool isDotEntry(const KIO::UDSEntry& entry)
{
    const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
    return name == QStringLiteral(".") || name == QStringLiteral("..");
}

void finishDirectoryItemListAtResourceLimit(KIO::ListJob* job,
    const kiriview::ImageIoJobCompletion& completion, const QUrl& directoryUrl,
    const kiriview::KioOperationFailureCallback& errorCallback)
{
    completion.claimAndRun([&]() {
        const QPointer<KIO::ListJob> guardedJob(job);
        job->kill(KJob::Quietly);
        kiriview::invokeIfSet(errorCallback,
            kiriview::kioOperationResourceLimitFailure(kiriview::KioOperationKind::DirectoryListing,
                directoryUrl,
                QStringLiteral("directory listing exceeds the configured resource limits")));
        if (!guardedJob.isNull()) {
            guardedJob->deleteLater();
        }
    });
}
}

namespace kiriview {
DirectoryItemListAdmission::DirectoryItemListAdmission(SiblingCandidateAdmissionLimits limits)
    : m_limits(limits)
{
    if (limits.maximumEntryCount < 0 || limits.maximumIdentityCodeUnitCount < 0) {
        reject();
    }
}

bool DirectoryItemListAdmission::admit(DirectoryItem item)
{
    if (m_rejected || std::cmp_greater_equal(m_items.size(), m_limits.maximumEntryCount)) {
        reject();
        return false;
    }

    const qsizetype nameCodeUnitCount = item.name.size();
    const qsizetype urlCodeUnitCount = item.url.toString(QUrl::FullyEncoded).size();
    if (nameCodeUnitCount < 0 || urlCodeUnitCount < 0
        || nameCodeUnitCount
            > m_limits.maximumIdentityCodeUnitCount - m_retainedIdentityCodeUnitCount) {
        reject();
        return false;
    }
    m_retainedIdentityCodeUnitCount += nameCodeUnitCount;
    if (urlCodeUnitCount
        > m_limits.maximumIdentityCodeUnitCount - m_retainedIdentityCodeUnitCount) {
        reject();
        return false;
    }
    m_retainedIdentityCodeUnitCount += urlCodeUnitCount;
    m_items.push_back(std::move(item));
    return true;
}

bool DirectoryItemListAdmission::rejected() const { return m_rejected; }

qsizetype DirectoryItemListAdmission::retainedEntryCount() const
{
    return static_cast<qsizetype>(m_items.size());
}

DirectoryItemList DirectoryItemListAdmission::takeItems()
{
    if (m_rejected) {
        return {};
    }
    return std::move(m_items);
}

void DirectoryItemListAdmission::reject()
{
    m_rejected = true;
    m_retainedIdentityCodeUnitCount = 0;
    DirectoryItemList().swap(m_items);
}

SiblingCandidateAdmissionLimits defaultSiblingCandidateAdmissionLimits()
{
    return SiblingCandidateAdmissionLimits { defaultMaximumSiblingEntryCount,
        defaultMaximumSiblingIdentityCodeUnitCount };
}

namespace {
    ImageIoJob startKioDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
        DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback,
        SiblingCandidateAdmissionLimits limits)
    {
        if (directoryUrl.isEmpty()) {
            warnDirectoryListingRejectedEmptyUrl();
            invokeIfSet(errorCallback,
                kioOperationValidationFailure(KioOperationKind::DirectoryListing, directoryUrl,
                    QStringLiteral("empty directory URL")));
            return {};
        }

        auto* listJob = KIO::listDir(directoryUrl, KIO::HideProgressInfo);
        listJob->setAutoDelete(false);
        if (receiver != nullptr) {
            listJob->setParent(receiver);
        }
        ImageIoJob ioJob(listJob, cancelDirectoryItemList);
        const ImageIoJobCompletion completion = ioJob.completion();
        auto admission = std::make_shared<DirectoryItemListAdmission>(limits);
        QObject* context = receiver == nullptr ? listJob : receiver;

        QObject::connect(listJob, &KIO::ListJob::entries, context,
            [admission, completion, directoryUrl, errorCallback](
                KIO::Job* emittedJob, const KIO::UDSEntryList& entries) {
                auto* currentJob = qobject_cast<KIO::ListJob*>(emittedJob);
                if (currentJob == nullptr || !completion.isActive()) {
                    return;
                }
                const QUrl listedDirectoryUrl = currentJob->url();
                for (const KIO::UDSEntry& entry : entries) {
                    if (isDotEntry(entry)) {
                        continue;
                    }
                    if (!admission->admit(directoryItem(entry, listedDirectoryUrl))) {
                        finishDirectoryItemListAtResourceLimit(
                            currentJob, completion, directoryUrl, errorCallback);
                        return;
                    }
                }
            });
        QObject::connect(listJob, &KJob::result, context,
            [admission, completion, directoryUrl, callback = std::move(callback), errorCallback](
                KJob* completedJob) mutable {
                if (completedJob->error() != 0) {
                    KioOperationFailure failure
                        = kioOperationFailureFromKJob(KioOperationKind::DirectoryListing,
                            directoryUrl, completedJob->error(), completedJob->errorString());
                    warnDirectoryListingJobFailure(directoryUrl, failure.diagnosticDetail);
                    completion.claimAndDelete(
                        [&]() { invokeIfSet(errorCallback, std::move(failure)); });
                    return;
                }

                completion.claimAndDelete([&]() { invokeIfSet(callback, admission->takeItems()); });
            });

        return ioJob;
    }
}

ImageIoJob startDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
    DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback)
{
    return startDirectoryItemList(receiver, directoryUrl, std::move(callback), errorCallback,
        defaultSiblingCandidateAdmissionLimits());
}

ImageIoJob startDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
    DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback,
    SiblingCandidateAdmissionLimits limits)
{
    return startKioDirectoryItemList(
        receiver, directoryUrl, std::move(callback), errorCallback, limits);
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

DirectoryItemListProvider defaultDirectoryItemListProvider(SiblingCandidateAdmissionLimits limits)
{
    return [limits](QObject* receiver, const QUrl& directoryUrl, DirectoryItemListCallback callback,
               const KioOperationFailureCallback& errorCallback) {
        return startKioDirectoryItemList(
            receiver, directoryUrl, std::move(callback), errorCallback, limits);
    };
}
}
