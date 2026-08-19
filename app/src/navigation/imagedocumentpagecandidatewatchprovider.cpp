// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatewatchprovider.h"

#include "async/imagecallback.h"
#include "imagedocumentpagecandidateitems.h"
#include "location/sourcekey.h"
#include "mediaformatregistry.h"
#include "system/kiooperationfailure.h"

#include <KDirWatch>
#include <QDir>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QSet>
#include <chrono>
#include <memory>
#include <utility>

namespace {
kiriview::ImageDocumentPageCandidateLoadError candidateAdmissionFailure(
    const QUrl& directoryUrl, kiriview::ImageDocumentPageCandidateAdmissionFailure failure)
{
    if (failure == kiriview::ImageDocumentPageCandidateAdmissionFailure::ScopeViolation) {
        return kiriview::ImageDocumentPageCandidateLoadError {
            kiriview::kioOperationValidationFailure(kiriview::KioOperationKind::DirectoryListing,
                directoryUrl,
                QStringLiteral(
                    "ordinary sibling listing returned a candidate outside the requested scope"))
        };
    }
    return kiriview::ImageDocumentPageCandidateLoadError {
        kiriview::kioOperationResourceLimitFailure(kiriview::KioOperationKind::DirectoryListing,
            directoryUrl,
            QStringLiteral("ordinary sibling listing exceeds the configured resource limits"))
    };
}

class LocalDirectoryChangeController final : public QObject
{
public:
    LocalDirectoryChangeController(QObject* parent, const QUrl& directoryUrl,
        kiriview::ImageDocumentPageCandidateDirectoryChangeCallback callback)
        : QObject(parent)
        , m_localDirectoryPath(QDir::cleanPath(directoryUrl.toLocalFile()))
        , m_callback(std::move(callback))
        , m_directoryWatch(std::make_unique<KDirWatch>())
        , m_fileSystemWatch(std::make_unique<QFileSystemWatcher>())
    {
        auto notify = [this](const QString&) { notifyChanged(); };
        QObject::connect(m_directoryWatch.get(), &KDirWatch::dirty, this, notify);
        QObject::connect(m_directoryWatch.get(), &KDirWatch::created, this, notify);
        QObject::connect(m_directoryWatch.get(), &KDirWatch::deleted, this, notify);
        m_directoryWatch->addDir(m_localDirectoryPath, KDirWatch::WatchDirOnly);
        QObject::connect(m_fileSystemWatch.get(), &QFileSystemWatcher::directoryChanged, this,
            [this](const QString&) { notifyChanged(); });
        rearm();
    }

    void rearm()
    {
        if (!m_localDirectoryPath.isEmpty()
            && !m_fileSystemWatch->directories().contains(m_localDirectoryPath)) {
            m_fileSystemWatch->addPath(m_localDirectoryPath);
        }
    }

    void cancel()
    {
        m_callback = {};
        m_directoryWatch->stopScan();
        deleteLater();
    }

private:
    void notifyChanged()
    {
        rearm();
        kiriview::invokeIfSet(m_callback);
    }

    QString m_localDirectoryPath;
    kiriview::ImageDocumentPageCandidateDirectoryChangeCallback m_callback;
    std::unique_ptr<KDirWatch> m_directoryWatch;
    std::unique_ptr<QFileSystemWatcher> m_fileSystemWatch;
};

void cancelLocalDirectoryChanges(QObject* object)
{
    static_cast<LocalDirectoryChangeController*>(object)->cancel();
}

kiriview::ImageDocumentPageCandidateDirectoryChangeSubscription subscribeToLocalDirectoryChanges(
    QObject* receiver, const QUrl& directoryUrl,
    kiriview::ImageDocumentPageCandidateDirectoryChangeCallback callback)
{
    if (!directoryUrl.isLocalFile()) {
        return {};
    }

    auto* controller
        = new LocalDirectoryChangeController(receiver, directoryUrl, std::move(callback));
    const QPointer<LocalDirectoryChangeController> guardedController(controller);
    return kiriview::ImageDocumentPageCandidateDirectoryChangeSubscription {
        kiriview::ImageIoJob(controller, cancelLocalDirectoryChanges),
        [guardedController]() {
            if (!guardedController.isNull()) {
                guardedController->rearm();
            }
        },
    };
}

class CandidateWatchController final : public QObject
{
public:
    CandidateWatchController(QObject* parent, QUrl directoryUrl,
        kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot,
        kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot,
        kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback,
        kiriview::ImageDocumentPageCandidateWatchDependencies dependencies,
        kiriview::SiblingCandidateAdmissionLimits limits)
        : QObject(parent)
        , m_directoryUrl(std::move(directoryUrl))
        , m_initialSnapshot(std::move(initialSnapshot))
        , m_changedSnapshot(std::move(changedSnapshot))
        , m_errorCallback(std::move(errorCallback))
        , m_directoryItemListProvider(std::move(dependencies.directoryItemListProvider))
        , m_directoryChangeProvider(std::move(dependencies.directoryChangeProvider))
        , m_limits(limits)
        , m_refreshTimer(
              kiriview::timerSchedulerWithDefaults(std::move(dependencies.timerScheduler))
                  .singleShotTimer(this, std::chrono::milliseconds(0), [this]() { runRefresh(); }))
    {
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        m_completion = std::move(completion);
    }

    void start()
    {
        if (m_directoryUrl.isLocalFile()) {
            const QPointer<CandidateWatchController> guardedThis(this);
            m_directoryChanges = m_directoryChangeProvider(this, m_directoryUrl, [guardedThis]() {
                if (!guardedThis.isNull()) {
                    guardedThis->requestRefresh();
                }
            });
        }
        requestRefresh();
    }

    void cancel()
    {
        if (m_canceled) {
            return;
        }
        m_canceled = true;
        m_refreshAdmission.invalidatePendingRefresh();
        m_refreshTimer->stop();
        m_directoryChanges.job.cancel();
        m_listing.cancel();
        m_initialSnapshot = {};
        m_changedSnapshot = {};
        m_errorCallback = {};
        deleteLater();
    }

private:
    void requestRefresh()
    {
        if (m_canceled) {
            return;
        }
        if (m_refreshAdmission.requestRefresh()) {
            m_scheduledEpoch = m_refreshAdmission.epoch();
            m_refreshTimer->start(std::chrono::milliseconds(0));
        }
    }

    void runRefresh()
    {
        if (m_canceled || !m_refreshAdmission.acceptsEpoch(m_scheduledEpoch)) {
            return;
        }
        m_refreshAdmission.beginRefresh();
        const quint64 refreshEpoch = m_scheduledEpoch;

        const QPointer<CandidateWatchController> guardedThis(this);
        auto listed
            = [guardedThis, refreshEpoch](const kiriview::DirectoryItemList& items) mutable {
                  if (!guardedThis.isNull()) {
                      guardedThis->handleItems(refreshEpoch, items);
                  }
              };
        auto failed = [guardedThis, refreshEpoch](kiriview::KioOperationFailure failure) mutable {
            if (!guardedThis.isNull()) {
                guardedThis->handleFailure(refreshEpoch,
                    kiriview::ImageDocumentPageCandidateLoadError { std::move(failure) });
            }
        };

        kiriview::ImageIoJob listing = kiriview::startDirectoryItemList(this, m_directoryUrl,
            std::move(listed), std::move(failed), m_directoryItemListProvider);
        if (guardedThis.isNull() || m_canceled) {
            listing.cancel();
            return;
        }
        m_listing = std::move(listing);
    }

    void handleItems(quint64 refreshEpoch, const kiriview::DirectoryItemList& items)
    {
        if (m_canceled || !m_refreshAdmission.acceptsEpoch(refreshEpoch)) {
            return;
        }
        kiriview::ImageDocumentPageCandidateAdmissionResult candidates
            = kiriview::imageDocumentPageNavigationCandidates(m_directoryUrl, items, m_limits);
        if (!candidates) {
            handleFailure(
                refreshEpoch, candidateAdmissionFailure(m_directoryUrl, candidates.error()));
            return;
        }

        m_freshness.noteSnapshot(items);
        m_freshness.apply(&*candidates);
        kiriview::invokeIfSet(m_directoryChanges.rearm);
        const bool initialAttempt = !std::exchange(m_initialAttemptHandled, true);
        const kiriview::ImageDocumentPageCandidateWatchSnapshotCallback callback
            = initialAttempt ? m_initialSnapshot : m_changedSnapshot;
        const QPointer<CandidateWatchController> guardedThis(this);
        kiriview::invokeIfSet(callback, std::move(*candidates));
        if (!guardedThis.isNull()) {
            finishRefresh();
        }
    }

    void handleFailure(quint64 refreshEpoch, kiriview::ImageDocumentPageCandidateLoadError failure)
    {
        if (m_canceled || !m_refreshAdmission.acceptsEpoch(refreshEpoch)) {
            return;
        }
        m_initialAttemptHandled = true;
        const kiriview::ImageDocumentPageCandidateLoadErrorCallback callback = m_errorCallback;
        const QPointer<CandidateWatchController> guardedThis(this);
        kiriview::invokeIfSet(callback, std::move(failure));
        if (!guardedThis.isNull()) {
            finishRefresh();
        }
    }

    void finishRefresh()
    {
        if (m_canceled) {
            return;
        }
        if (!m_directoryChanges.job.isActive()) {
            m_completion.claimAndDelete([]() { });
            return;
        }
        if (m_refreshAdmission.finishRefresh()) {
            m_scheduledEpoch = m_refreshAdmission.epoch();
            m_refreshTimer->start(std::chrono::milliseconds(0));
        }
    }

    QUrl m_directoryUrl;
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback m_initialSnapshot;
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback m_changedSnapshot;
    kiriview::ImageDocumentPageCandidateLoadErrorCallback m_errorCallback;
    kiriview::DirectoryItemListProvider m_directoryItemListProvider;
    kiriview::ImageDocumentPageCandidateDirectoryChangeProvider m_directoryChangeProvider;
    kiriview::ImageDocumentPageCandidateDirectoryChangeSubscription m_directoryChanges;
    kiriview::SiblingCandidateAdmissionLimits m_limits;
    kiriview::ImageDocumentPageCandidateRefreshAdmission m_refreshAdmission;
    kiriview::ImageDocumentPageCandidateFreshnessState m_freshness;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::ImageIoJob m_listing;
    std::unique_ptr<kiriview::RuntimeTimerHandle> m_refreshTimer;
    quint64 m_scheduledEpoch = 0;
    bool m_initialAttemptHandled = false;
    bool m_canceled = false;
};

void cancelCandidateWatch(QObject* object)
{
    static_cast<CandidateWatchController*>(object)->cancel();
}

kiriview::ImageIoJob startImageDocumentPageCandidateWatch(QObject* receiver,
    const QUrl& directoryUrl,
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot,
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback,
    kiriview::ImageDocumentPageCandidateWatchDependencies dependencies,
    kiriview::SiblingCandidateAdmissionLimits limits)
{
    if (directoryUrl.isEmpty()) {
        kiriview::invokeIfSet(errorCallback,
            kiriview::ImageDocumentPageCandidateLoadError { kiriview::kioOperationValidationFailure(
                kiriview::KioOperationKind::DirectoryListing, directoryUrl,
                QStringLiteral("directory candidate watch URL was rejected")) });
        return {};
    }

    auto* controller
        = new CandidateWatchController(receiver, directoryUrl, std::move(initialSnapshot),
            std::move(changedSnapshot), std::move(errorCallback), std::move(dependencies), limits);
    kiriview::ImageIoJob job(controller, cancelCandidateWatch);
    controller->setCompletion(job.completion());
    controller->start();
    return job;
}
}

namespace kiriview {
void ImageDocumentPageCandidateFreshnessState::noteSnapshot(const DirectoryItemList& items)
{
    QHash<QString, SourceVersion> nextVersionBySource;
    for (const DirectoryItem& item : items) {
        if (!item.isFile || !isSupportedOrdinaryMediaFileName(item.name)) {
            continue;
        }
        const SourceKey sourceKey = sourceKeyForUrl(item.url);
        if (!sourceKey.valid) {
            continue;
        }

        const SourceVersion version { item.byteSize, item.modificationTimeSeconds };
        nextVersionBySource.insert(sourceKey.identity, version);
        if (m_snapshotRecorded
            && (!m_lastVersionBySource.contains(sourceKey.identity)
                || m_lastVersionBySource.value(sourceKey.identity) != version)) {
            noteItem(item);
        }
    }
    m_lastVersionBySource = std::move(nextVersionBySource);
    m_snapshotRecorded = true;
}

void ImageDocumentPageCandidateFreshnessState::apply(
    std::vector<ImageDocumentPageCandidate>* candidates)
{
    if (candidates == nullptr) {
        return;
    }

    QSet<QString> retainedSources;
    for (ImageDocumentPageCandidate& candidate : *candidates) {
        const SourceKey sourceKey = sourceKeyForUrl(candidate.url);
        candidate.sourceFreshness
            = sourceKey.valid ? m_freshnessBySource.value(sourceKey.identity, 0) : 0;
        if (sourceKey.valid) {
            retainedSources.insert(sourceKey.identity);
        }
    }
    m_freshnessBySource.removeIf([&retainedSources](const auto& freshness) {
        return !retainedSources.contains(freshness.key());
    });
}

void ImageDocumentPageCandidateFreshnessState::noteItem(const DirectoryItem& item)
{
    if (!item.isFile || !isSupportedOrdinaryMediaFileName(item.name)) {
        return;
    }
    const SourceKey sourceKey = sourceKeyForUrl(item.url);
    if (!sourceKey.valid) {
        return;
    }

    ++m_nextFreshness;
    if (m_nextFreshness == 0) {
        ++m_nextFreshness;
    }
    m_freshnessBySource.insert(sourceKey.identity, m_nextFreshness);
}

bool ImageDocumentPageCandidateRefreshAdmission::requestRefresh()
{
    m_refreshRequested = true;
    if (m_refreshPending) {
        return false;
    }

    m_refreshPending = true;
    return true;
}

void ImageDocumentPageCandidateRefreshAdmission::beginRefresh() { m_refreshRequested = false; }

bool ImageDocumentPageCandidateRefreshAdmission::finishRefresh()
{
    if (m_refreshRequested) {
        return true;
    }

    m_refreshPending = false;
    return false;
}

void ImageDocumentPageCandidateRefreshAdmission::invalidatePendingRefresh()
{
    m_refreshPending = false;
    m_refreshRequested = false;
    ++m_epoch;
}

quint64 ImageDocumentPageCandidateRefreshAdmission::epoch() const { return m_epoch; }

bool ImageDocumentPageCandidateRefreshAdmission::acceptsEpoch(quint64 epoch) const
{
    return epoch == m_epoch;
}

ImageDocumentPageCandidateWatchProvider defaultImageDocumentPageCandidateWatchProvider(
    ImageDocumentPageCandidateWatchDependencies dependencies,
    SiblingCandidateAdmissionLimits limits)
{
    if (!dependencies.directoryItemListProvider) {
        dependencies.directoryItemListProvider = defaultDirectoryItemListProvider(limits);
    }
    if (!dependencies.directoryChangeProvider) {
        dependencies.directoryChangeProvider = subscribeToLocalDirectoryChanges;
    }
    dependencies.timerScheduler
        = timerSchedulerWithDefaults(std::move(dependencies.timerScheduler));

    return [dependencies = std::move(dependencies), limits](QObject* receiver,
               const QUrl& directoryUrl,
               ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot,
               ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot,
               ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
        return startImageDocumentPageCandidateWatch(receiver, directoryUrl,
            std::move(initialSnapshot), std::move(changedSnapshot), std::move(errorCallback),
            dependencies, limits);
    };
}
}
