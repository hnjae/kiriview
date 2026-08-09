// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatestoreentrystate.h"

#include "location/imageurl.h"

#include <algorithm>
#include <utility>

namespace {
bool sameImageDocumentPageCandidates(const std::vector<kiriview::ImageDocumentPageCandidate>& left,
    const std::vector<kiriview::ImageDocumentPageCandidate>& right)
{
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!kiriview::sameNormalizedUrl(left[index].url, right[index].url)
            || left[index].name != right[index].name) {
            return false;
        }
    }

    return true;
}

template <typename Item> void pruneInactiveItems(std::vector<Item>* items)
{
    std::erase_if(*items, [](const Item& item) { return !item.completion.isActive(); });
}
}

namespace kiriview {
const std::vector<ImageDocumentPageCandidate>&
ImageDocumentPageCandidateStoreEntryState::candidates() const
{
    return m_candidates;
}

bool ImageDocumentPageCandidateStoreEntryState::listed() const { return m_listed; }

bool ImageDocumentPageCandidateStoreEntryState::failed() const { return m_failed; }

const ImageDocumentPageCandidateLoadError& ImageDocumentPageCandidateStoreEntryState::error() const
{
    return m_error;
}

bool ImageDocumentPageCandidateStoreEntryState::hasActiveClients()
{
    std::erase_if(m_pendingLoads, [](const ImageDocumentPageCandidateStoreEntryPendingLoad& load) {
        return !load.completion.isActive();
    });
    pruneInactiveItems(&m_subscribers);
    return !m_pendingLoads.empty() || !m_subscribers.empty();
}

void ImageDocumentPageCandidateStoreEntryState::addPendingLoad(ImageIoJobCompletion completion,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    m_pendingLoads.push_back(ImageDocumentPageCandidateStoreEntryPendingLoad {
        std::move(completion),
        std::move(callback),
        std::move(errorCallback),
    });
}

void ImageDocumentPageCandidateStoreEntryState::addSubscriber(ImageIoJobCompletion completion,
    ImageDocumentPageCandidatesCallback callback,
    ImageDocumentPageCandidateLoadErrorCallback errorCallback)
{
    m_subscribers.push_back(ImageDocumentPageCandidateStoreEntrySubscriber {
        std::move(completion),
        std::move(callback),
        std::move(errorCallback),
    });
}

void ImageDocumentPageCandidateStoreEntryState::removePendingLoad(QObject* token)
{
    std::erase_if(
        m_pendingLoads, [token](const ImageDocumentPageCandidateStoreEntryPendingLoad& load) {
            return load.completion.object() == token;
        });
}

void ImageDocumentPageCandidateStoreEntryState::removeSubscriber(QObject* token)
{
    std::erase_if(
        m_subscribers, [token](const ImageDocumentPageCandidateStoreEntrySubscriber& subscriber) {
            return subscriber.completion.object() == token;
        });
}

ImageDocumentPageCandidateStoreEntryNotificationPlan
ImageDocumentPageCandidateStoreEntryState::completeListing(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    const bool wasListed = m_listed;
    const bool wasFailed = m_failed;
    const bool changed = replaceCandidates(std::move(candidates));
    m_listed = true;
    m_failed = false;
    m_error = QString();

    ImageDocumentPageCandidateStoreEntryNotificationPlan plan;
    plan.completedLoads = takePendingLoads();
    plan.candidates = m_candidates;
    if (wasFailed || (wasListed && changed)) {
        plan.changedSubscribers = activeSubscribers();
    }
    return plan;
}

ImageDocumentPageCandidateStoreEntryNotificationPlan
ImageDocumentPageCandidateStoreEntryState::updateListing(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    const bool wasListed = m_listed;
    const bool wasFailed = m_failed;
    const bool changed = replaceCandidates(std::move(candidates));
    m_listed = true;
    m_failed = false;
    m_error = QString();
    ImageDocumentPageCandidateStoreEntryNotificationPlan plan;
    plan.candidates = m_candidates;
    if (!wasListed || wasFailed || changed) {
        plan.changedSubscribers = activeSubscribers();
    }
    return plan;
}

ImageDocumentPageCandidateStoreEntryNotificationPlan
ImageDocumentPageCandidateStoreEntryState::failListing(ImageDocumentPageCandidateLoadError error)
{
    m_candidates.clear();
    m_failed = true;
    m_error = std::move(error);

    ImageDocumentPageCandidateStoreEntryNotificationPlan plan;
    plan.failedLoads = takePendingLoads();
    plan.failedSubscribers = activeSubscribers();
    plan.error = m_error;
    return plan;
}

bool ImageDocumentPageCandidateStoreEntryState::replaceCandidates(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    if (sameImageDocumentPageCandidates(m_candidates, candidates)) {
        return false;
    }

    m_candidates = std::move(candidates);
    return true;
}

std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad>
ImageDocumentPageCandidateStoreEntryState::takePendingLoads()
{
    std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad> pendingLoads
        = std::move(m_pendingLoads);
    m_pendingLoads.clear();
    return pendingLoads;
}

std::vector<ImageDocumentPageCandidateStoreEntrySubscriber>
ImageDocumentPageCandidateStoreEntryState::activeSubscribers()
{
    pruneInactiveItems(&m_subscribers);
    return m_subscribers;
}
}
