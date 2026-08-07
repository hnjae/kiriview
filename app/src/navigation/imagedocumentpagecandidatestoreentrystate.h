// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATESTOREENTRYSTATE_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATESTOREENTRYSTATE_H

#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "imagedocumentpagecandidatecallbacks.h"
#include "imagedocumentpagecandidateloaderror.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QPointer>
#include <QString>
#include <vector>

class QObject;

namespace kiriview {
struct ImageDocumentPageCandidateStoreEntryPendingLoad
{
    ImageIoJobCompletion completion;
    ImageDocumentPageCandidatesCallback callback;
    ImageDocumentPageCandidateLoadErrorCallback errorCallback;
};

struct ImageDocumentPageCandidateStoreEntrySubscriber
{
    ImageIoJobCompletion completion;
    ImageDocumentPageCandidatesCallback callback;
    ImageDocumentPageCandidateLoadErrorCallback errorCallback;
};

struct ImageDocumentPageCandidateStoreEntryNotificationPlan
{
    std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad> completedLoads;
    std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad> failedLoads;
    std::vector<ImageDocumentPageCandidateStoreEntrySubscriber> changedSubscribers;
    std::vector<ImageDocumentPageCandidateStoreEntrySubscriber> failedSubscribers;
    std::vector<ImageDocumentPageCandidate> candidates;
    ImageDocumentPageCandidateLoadError error = QString();
};

class ImageDocumentPageCandidateStoreEntryState final
{
public:
    [[nodiscard]] const std::vector<ImageDocumentPageCandidate>& candidates() const;
    [[nodiscard]] bool listed() const;
    [[nodiscard]] bool failed() const;
    [[nodiscard]] const ImageDocumentPageCandidateLoadError& error() const;
    [[nodiscard]] bool hasActiveClients();

    void addPendingLoad(ImageIoJobCompletion completion,
        ImageDocumentPageCandidatesCallback callback,
        ImageDocumentPageCandidateLoadErrorCallback errorCallback);
    void addSubscriber(ImageIoJobCompletion completion,
        ImageDocumentPageCandidatesCallback callback,
        ImageDocumentPageCandidateLoadErrorCallback errorCallback);
    void removePendingLoad(QObject* token);
    void removeSubscriber(QObject* token);

    ImageDocumentPageCandidateStoreEntryNotificationPlan completeListing(
        std::vector<ImageDocumentPageCandidate> candidates);
    ImageDocumentPageCandidateStoreEntryNotificationPlan updateListing(
        std::vector<ImageDocumentPageCandidate> candidates);
    ImageDocumentPageCandidateStoreEntryNotificationPlan failListing(
        ImageDocumentPageCandidateLoadError error);

private:
    bool replaceCandidates(std::vector<ImageDocumentPageCandidate> candidates);
    std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad> takePendingLoads();
    std::vector<ImageDocumentPageCandidateStoreEntrySubscriber> activeSubscribers();

    std::vector<ImageDocumentPageCandidate> m_candidates;
    std::vector<ImageDocumentPageCandidateStoreEntryPendingLoad> m_pendingLoads;
    std::vector<ImageDocumentPageCandidateStoreEntrySubscriber> m_subscribers;
    bool m_listed = false;
    bool m_failed = false;
    ImageDocumentPageCandidateLoadError m_error = QString();
};
}

#endif
