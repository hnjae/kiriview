// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEDIRECTORYENTRY_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEDIRECTORYENTRY_H

#include "async/imageasynccallbacks.h"
#include "imagedocumentpagecandidatecallbacks.h"
#include "imagedocumentpagecandidatestoreentrystate.h"
#include "imagedocumentpagecandidatewatchprovider.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <cstdint>
#include <functional>
#include <vector>

class QObject;

namespace kiriview {
class ImageDocumentPageCandidateDirectoryEntry final
{
public:
    ImageDocumentPageCandidateDirectoryEntry(QUrl directoryUrl,
        ImageDocumentPageCandidateWatchProvider watchProvider, QObject* signalContext,
        std::uint64_t identity = 0, std::function<void()> idleCallback = {});
    ~ImageDocumentPageCandidateDirectoryEntry();
    Q_DISABLE_COPY_MOVE(ImageDocumentPageCandidateDirectoryEntry)

    [[nodiscard]] bool failed() const;
    [[nodiscard]] bool listed() const;
    [[nodiscard]] bool watching() const;
    [[nodiscard]] const ImageDocumentPageCandidateLoadError& error() const;
    [[nodiscard]] const std::vector<ImageDocumentPageCandidate>& candidates() const;
    [[nodiscard]] std::uint64_t identity() const;
    [[nodiscard]] bool hasActiveClients();

    bool open();
    void handleCompleted(std::vector<ImageDocumentPageCandidate> candidates);
    void handleChanged(std::vector<ImageDocumentPageCandidate> candidates);
    void handleDeleted(const QList<QUrl>& urls);
    void handleError(ImageDocumentPageCandidateLoadError error);

    ImageIoJob addPendingLoad(ImageDocumentPageCandidatesCallback callback,
        ImageDocumentPageCandidateLoadErrorCallback errorCallback, QObject* receiver,
        std::function<void(QObject*)> removeToken);
    ImageIoJob addSubscriber(ImageDocumentPageCandidatesCallback callback,
        ImageDocumentPageCandidateLoadErrorCallback errorCallback, QObject* receiver,
        std::function<void(QObject*)> removeToken);
    void removePendingLoad(QObject* token);
    void removeSubscriber(QObject* token);

private:
    void reportIdle();

    QUrl m_directoryUrl;
    ImageDocumentPageCandidateWatchProvider m_watchProvider;
    QPointer<QObject> m_signalContext;
    std::uint64_t m_identity = 0;
    std::function<void()> m_idleCallback;
    ImageIoJob m_watchJob;
    ImageDocumentPageCandidateStoreEntryState m_state;
};
}

#endif
