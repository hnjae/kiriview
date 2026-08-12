// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatewatchprovider.h"

#include "async/imagecallback.h"
#include "imagedocumentpagecandidateitems.h"
#include "location/sourcekey.h"
#include "mediaformatregistry.h"
#include "system/kiooperationfailure.h"

#include <KCoreDirLister>
#include <KIO/Job>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <chrono>
#include <memory>
#include <utility>

namespace {
using namespace std::chrono_literals;

constexpr const char* CanceledProperty = "_kiriviewCanceledImageDocumentPageCandidateWatch";

void cancelLiveImageDocumentPageCandidateLister(QObject* object)
{
    auto* lister = qobject_cast<KCoreDirLister*>(object);
    if (lister == nullptr) {
        return;
    }

    lister->setProperty(CanceledProperty, true);
    QObject::disconnect(lister, nullptr, nullptr, nullptr);
    lister->stop();
    lister->deleteLater();
}

KCoreDirLister* createLiveImageDocumentPageCandidateLister(QObject* parent)
{
    auto* lister = new KCoreDirLister(parent);
    lister->setAutoErrorHandlingEnabled(false);
    lister->setAutoUpdate(true);
    lister->setDelayedMimeTypes(true);
    lister->setShowHiddenFiles(true);
    return lister;
}

kiriview::ImageDocumentPageCandidateAdmissionResult imageDocumentPageCandidatesForLister(
    KCoreDirLister* lister, const QUrl& directoryUrl)
{
    return kiriview::imageDocumentPageNavigationCandidates(
        directoryUrl, lister->itemsForDir(directoryUrl, KCoreDirLister::AllItems));
}

bool watchCanceled(const KCoreDirLister* lister)
{
    return lister == nullptr || lister->property(CanceledProperty).toBool();
}

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

struct CandidateWatchRefreshState
{
    QPointer<KCoreDirLister> lister;
    QPointer<QTimer> refreshTimer;
    QUrl directoryUrl;
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot;
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback;
    kiriview::ImageDocumentPageCandidateRefreshAdmission admission;
    kiriview::ImageDocumentPageCandidateFreshnessState freshness;
    quint64 scheduledEpoch = 0;
    bool initialSnapshotHandled = false;
};

void scheduleChangedSnapshotRefresh(const std::shared_ptr<CandidateWatchRefreshState>& state);

void runChangedSnapshotRefresh(const std::shared_ptr<CandidateWatchRefreshState>& state)
{
    if (state->lister.isNull() || watchCanceled(state->lister.data())) {
        return;
    }
    if (!state->admission.acceptsEpoch(state->scheduledEpoch)) {
        return;
    }

    state->admission.beginRefresh();
    kiriview::ImageDocumentPageCandidateAdmissionResult candidates
        = imageDocumentPageCandidatesForLister(state->lister.data(), state->directoryUrl);
    if (candidates) {
        state->freshness.apply(&*candidates);
        kiriview::invokeIfSet(state->changedSnapshot, std::move(*candidates));
    } else {
        kiriview::invokeIfSet(state->errorCallback,
            candidateAdmissionFailure(state->directoryUrl, candidates.error()));
    }

    if (state->admission.finishRefresh()) {
        scheduleChangedSnapshotRefresh(state);
    }
}

void scheduleChangedSnapshotRefresh(const std::shared_ptr<CandidateWatchRefreshState>& state)
{
    if (state->lister.isNull() || state->refreshTimer.isNull()) {
        return;
    }

    state->scheduledEpoch = state->admission.epoch();
    state->refreshTimer->start(0ms);
}

void requestChangedSnapshotRefresh(const std::shared_ptr<CandidateWatchRefreshState>& state)
{
    if (!state->initialSnapshotHandled || state->lister.isNull()
        || watchCanceled(state->lister.data())) {
        return;
    }

    if (state->admission.requestRefresh()) {
        scheduleChangedSnapshotRefresh(state);
    }
}

kiriview::ImageIoJob startKCoreImageDocumentPageCandidateWatch(QObject* receiver,
    const QUrl& directoryUrl,
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot,
    const kiriview::ImageDocumentPageCandidateWatchSnapshotCallback& changedSnapshot,
    const kiriview::ImageDocumentPageCandidateWatchDeletedCallback&,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    auto* lister = createLiveImageDocumentPageCandidateLister(receiver);
    kiriview::ImageIoJob ioJob(lister, cancelLiveImageDocumentPageCandidateLister);
    QObject* context = receiver == nullptr ? lister : receiver;
    auto refreshState = std::make_shared<CandidateWatchRefreshState>();
    refreshState->lister = lister;
    refreshState->refreshTimer = new QTimer(lister);
    refreshState->refreshTimer->setSingleShot(true);
    refreshState->directoryUrl = directoryUrl;
    refreshState->changedSnapshot = changedSnapshot;
    refreshState->errorCallback = errorCallback;
    QObject::connect(refreshState->refreshTimer.data(), &QTimer::timeout, lister,
        [refreshState]() { runChangedSnapshotRefresh(refreshState); });

    QObject::connect(lister, &KCoreDirLister::completed, context,
        [refreshState, initialSnapshot = std::move(initialSnapshot)]() mutable {
            if (refreshState->initialSnapshotHandled || refreshState->lister.isNull()
                || watchCanceled(refreshState->lister.data())) {
                return;
            }

            refreshState->initialSnapshotHandled = true;
            kiriview::ImageDocumentPageCandidateAdmissionResult candidates
                = imageDocumentPageCandidatesForLister(
                    refreshState->lister.data(), refreshState->directoryUrl);
            if (candidates) {
                refreshState->freshness.apply(&*candidates);
                kiriview::invokeIfSet(initialSnapshot, std::move(*candidates));
            } else {
                kiriview::invokeIfSet(refreshState->errorCallback,
                    candidateAdmissionFailure(refreshState->directoryUrl, candidates.error()));
            }
        });
    QObject::connect(lister, &KCoreDirLister::itemsAdded, context,
        [refreshState](const QUrl&, const KFileItemList& items) {
            if (refreshState->initialSnapshotHandled) {
                refreshState->freshness.noteAddedItems(items);
            }
            requestChangedSnapshotRefresh(refreshState);
        });
    QObject::connect(
        lister, &KCoreDirLister::itemsDeleted, context, [refreshState](const KFileItemList& items) {
            if (refreshState->lister.isNull() || watchCanceled(refreshState->lister.data())) {
                return;
            }

            if (refreshState->initialSnapshotHandled) {
                refreshState->freshness.noteDeletedItems(items);
            }
            requestChangedSnapshotRefresh(refreshState);
        });
    QObject::connect(lister, &KCoreDirLister::refreshItems, context,
        [refreshState](const QList<QPair<KFileItem, KFileItem>>& items) {
            if (refreshState->initialSnapshotHandled) {
                refreshState->freshness.noteRefreshedItems(items);
            }
            requestChangedSnapshotRefresh(refreshState);
        });
    QObject::connect(lister, &KCoreDirLister::clear, context,
        [refreshState]() { requestChangedSnapshotRefresh(refreshState); });
    QObject::connect(lister, &KCoreDirLister::clearDir, context,
        [refreshState](const QUrl&) { requestChangedSnapshotRefresh(refreshState); });
    QObject::connect(
        lister, &KCoreDirLister::jobError, context, [refreshState](KIO::Job* job) mutable {
            if (refreshState->lister.isNull() || watchCanceled(refreshState->lister.data())) {
                return;
            }

            refreshState->initialSnapshotHandled = true;
            refreshState->admission.invalidatePendingRefresh();
            if (!refreshState->refreshTimer.isNull()) {
                refreshState->refreshTimer->stop();
            }
            kiriview::KioOperationFailure failure = job == nullptr
                ? kiriview::kioOperationValidationFailure(
                      kiriview::KioOperationKind::DirectoryListing, refreshState->directoryUrl,
                      QStringLiteral("directory candidate watch emitted a null job"))
                : kiriview::kioOperationFailureFromKJob(
                      kiriview::KioOperationKind::DirectoryListing, refreshState->directoryUrl,
                      job->error(), job->errorString());
            kiriview::invokeIfSet(refreshState->errorCallback,
                kiriview::ImageDocumentPageCandidateLoadError { std::move(failure) });
        });

    if (!lister->openUrl(directoryUrl, KCoreDirLister::Reload)) {
        kiriview::invokeIfSet(errorCallback,
            kiriview::ImageDocumentPageCandidateLoadError { kiriview::kioOperationValidationFailure(
                kiriview::KioOperationKind::DirectoryListing, directoryUrl,
                QStringLiteral("directory candidate watch URL was rejected")) });
        ioJob.cancel();
        return kiriview::ImageIoJob();
    }

    return ioJob;
}
}

namespace kiriview {
void ImageDocumentPageCandidateFreshnessState::noteAddedItems(const KFileItemList& items)
{
    for (const KFileItem& item : items) {
        noteItem(item);
    }
}

void ImageDocumentPageCandidateFreshnessState::noteDeletedItems(const KFileItemList& items)
{
    for (const KFileItem& item : items) {
        noteItem(item);
    }
}

void ImageDocumentPageCandidateFreshnessState::noteRefreshedItems(
    const QList<QPair<KFileItem, KFileItem>>& items)
{
    for (const auto& [oldItem, newItem] : items) {
        noteItem(oldItem);
        noteItem(newItem);
    }
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

void ImageDocumentPageCandidateFreshnessState::noteItem(const KFileItem& item)
{
    if (!isSupportedOrdinaryMediaFileName(item.name())) {
        return;
    }
    const SourceKey sourceKey = sourceKeyForUrl(item.url());
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

ImageDocumentPageCandidateWatchProvider defaultImageDocumentPageCandidateWatchProvider()
{
    return [](QObject* receiver, const QUrl& directoryUrl,
               ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot,
               const ImageDocumentPageCandidateWatchSnapshotCallback& changedSnapshot,
               const ImageDocumentPageCandidateWatchDeletedCallback& deletedUrls,
               ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
        return startKCoreImageDocumentPageCandidateWatch(receiver, directoryUrl,
            std::move(initialSnapshot), changedSnapshot, deletedUrls, std::move(errorCallback));
    };
}
}
