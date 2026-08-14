// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatedirectoryentry.h"

#include "async/imagecallback.h"
#include <memory>
#include <utility>

namespace {
QObject* createEntryJobToken(QObject* receiver, QObject* fallbackParent)
{
    return new QObject(receiver == nullptr ? fallbackParent : receiver);
}

kiriview::ImageIoJob createEntryJob(
    QObject* token, QObject* signalContext, std::function<void(QObject*)> removeToken)
{
    auto removalNotified = std::make_shared<bool>(false);
    auto notifyRemoval = [removeToken = std::move(removeToken), removalNotified](QObject* object) {
        if (*removalNotified) {
            return;
        }
        *removalNotified = true;
        removeToken(object);
    };
    QObject::connect(token, &QObject::destroyed, signalContext,
        [notifyRemoval](QObject* object) mutable { notifyRemoval(object); });
    return kiriview::ImageIoJob(
        token, [notifyRemoval = std::move(notifyRemoval)](QObject* object) mutable {
            notifyRemoval(object);
            object->deleteLater();
        });
}

void applyEntryNotificationPlan(kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan plan,
    const QPointer<QObject>& signalContext)
{
    for (const kiriview::ImageDocumentPageCandidateStoreEntryPendingLoad& load :
        plan.completedLoads) {
        if (signalContext.isNull()) {
            return;
        }
        load.completion.claimAndDelete(
            [&]() { kiriview::invokeIfSet(load.callback, plan.candidates); });
    }
    for (const kiriview::ImageDocumentPageCandidateStoreEntrySubscriber& subscriber :
        plan.changedSubscribers) {
        if (signalContext.isNull()) {
            return;
        }
        if (subscriber.completion.isActive()) {
            kiriview::invokeIfSet(subscriber.callback, plan.candidates);
        }
    }
    for (const kiriview::ImageDocumentPageCandidateStoreEntryPendingLoad& load : plan.failedLoads) {
        if (signalContext.isNull()) {
            return;
        }
        load.completion.claimAndDelete(
            [&]() { kiriview::invokeIfSet(load.errorCallback, plan.error); });
    }
    for (const kiriview::ImageDocumentPageCandidateStoreEntrySubscriber& subscriber :
        plan.failedSubscribers) {
        if (signalContext.isNull()) {
            return;
        }
        if (subscriber.completion.isActive()) {
            kiriview::invokeIfSet(subscriber.errorCallback, plan.error);
        }
    }
}
}

namespace kiriview {
ImageDocumentPageCandidateDirectoryEntry::ImageDocumentPageCandidateDirectoryEntry(
    QUrl directoryUrl, ImageDocumentPageCandidateWatchProvider watchProvider,
    QObject* signalContext, std::uint64_t identity, std::function<void()> idleCallback)
    : m_directoryUrl(std::move(directoryUrl))
    , m_watchProvider(std::move(watchProvider))
    , m_signalContext(signalContext)
    , m_identity(identity)
    , m_idleCallback(std::move(idleCallback))
{
    if (!m_watchProvider) {
        m_watchProvider = defaultImageDocumentPageCandidateWatchProvider();
    }
}

ImageDocumentPageCandidateDirectoryEntry::~ImageDocumentPageCandidateDirectoryEntry()
{
    m_watchJob.cancel();
}

bool ImageDocumentPageCandidateDirectoryEntry::failed() const { return m_state.failed(); }

bool ImageDocumentPageCandidateDirectoryEntry::listed() const { return m_state.listed(); }

bool ImageDocumentPageCandidateDirectoryEntry::watching() const { return m_watchJob.isActive(); }

const ImageDocumentPageCandidateLoadError& ImageDocumentPageCandidateDirectoryEntry::error() const
{
    return m_state.error();
}

const std::vector<ImageDocumentPageCandidate>&
ImageDocumentPageCandidateDirectoryEntry::candidates() const
{
    return m_state.candidates();
}

std::uint64_t ImageDocumentPageCandidateDirectoryEntry::identity() const { return m_identity; }

bool ImageDocumentPageCandidateDirectoryEntry::hasActiveClients()
{
    return m_state.hasActiveClients();
}

bool ImageDocumentPageCandidateDirectoryEntry::open()
{
    if (m_watchJob.isActive() || !m_watchProvider) {
        return m_watchJob.isActive();
    }

    m_watchJob = m_watchProvider(
        m_signalContext, m_directoryUrl,
        [this](std::vector<ImageDocumentPageCandidate> candidates) {
            handleCompleted(std::move(candidates));
        },
        [this](std::vector<ImageDocumentPageCandidate> candidates) {
            handleChanged(std::move(candidates));
        },
        [this](ImageDocumentPageCandidateLoadError error) { handleError(std::move(error)); });
    return m_watchJob.isActive();
}

void ImageDocumentPageCandidateDirectoryEntry::handleCompleted(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = m_state.completeListing(std::move(candidates));
    const QPointer<QObject> signalContext = m_signalContext;
    reportIdle();
    applyEntryNotificationPlan(std::move(plan), signalContext);
}

void ImageDocumentPageCandidateDirectoryEntry::handleChanged(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = m_state.updateListing(std::move(candidates));
    const QPointer<QObject> signalContext = m_signalContext;
    reportIdle();
    applyEntryNotificationPlan(std::move(plan), signalContext);
}

void ImageDocumentPageCandidateDirectoryEntry::handleError(
    ImageDocumentPageCandidateLoadError error)
{
    ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = m_state.failListing(std::move(error));
    const QPointer<QObject> signalContext = m_signalContext;
    reportIdle();
    applyEntryNotificationPlan(std::move(plan), signalContext);
}

ImageIoJob ImageDocumentPageCandidateDirectoryEntry::addPendingLoad(
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback, QObject* receiver,
    std::function<void(QObject*)> removeToken)
{
    QObject* token = createEntryJobToken(receiver, m_signalContext);
    ImageIoJob job = createEntryJob(token, m_signalContext, std::move(removeToken));
    m_state.addPendingLoad(job.completion(), std::move(callback), std::move(errorCallback));
    return job;
}

ImageIoJob ImageDocumentPageCandidateDirectoryEntry::addSubscriber(
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback, QObject* receiver,
    std::function<void(QObject*)> removeToken)
{
    QObject* token = createEntryJobToken(receiver, m_signalContext);
    ImageIoJob job = createEntryJob(token, m_signalContext, std::move(removeToken));
    m_state.addSubscriber(job.completion(), std::move(callback), std::move(errorCallback));
    return job;
}

void ImageDocumentPageCandidateDirectoryEntry::removePendingLoad(QObject* token)
{
    m_state.removePendingLoad(token);
    reportIdle();
}

void ImageDocumentPageCandidateDirectoryEntry::removeSubscriber(QObject* token)
{
    m_state.removeSubscriber(token);
    reportIdle();
}

void ImageDocumentPageCandidateDirectoryEntry::reportIdle()
{
    if (!m_state.hasActiveClients() && m_idleCallback) {
        m_idleCallback();
    }
}
}
