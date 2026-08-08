// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONRUNTIME_H

#include "navigation/directmedianavigationcandidateprovider.h"
#include "navigation/directmedianavigationmodel.h"
#include "session/documentsessiondirectmedianavigationloadstate.h"

#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct DocumentSessionDirectMediaNavigationCandidatesResult
{
    std::vector<DirectMediaNavigationCandidate> candidates;
    bool succeeded = false;
    QString errorString;
    std::optional<KioOperationFailure> failure;
};

struct DocumentSessionDirectMediaNavigationRefreshResult
{
    std::vector<DirectMediaNavigationCandidate> candidates;
    DirectMediaNavigationBoundaryState boundaryState;
    bool succeeded = false;
    QString errorString;
    std::optional<KioOperationFailure> failure;
};

struct DocumentSessionDirectMediaNavigationOpenResult
{
    std::vector<DirectMediaNavigationCandidate> candidates;
    DirectMediaNavigationOpenPlan plan;
    bool succeeded = false;
    QString errorString;
    std::optional<KioOperationFailure> failure;
};

class DocumentSessionDirectMediaNavigationRuntime final
{
public:
    using ScopeAccepted = std::function<bool(const DirectMediaScope&)>;
    using CandidatesCallback
        = std::function<void(DocumentSessionDirectMediaNavigationCandidatesResult)>;
    using RefreshCallback = std::function<void(DocumentSessionDirectMediaNavigationRefreshResult)>;
    using OpenCallback = std::function<void(DocumentSessionDirectMediaNavigationOpenResult)>;

    explicit DocumentSessionDirectMediaNavigationRuntime(
        DirectMediaNavigationCandidateProvider provider = {});
    ~DocumentSessionDirectMediaNavigationRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionDirectMediaNavigationRuntime)

    void loadCandidates(QObject* receiver, const DirectMediaScope& scope,
        ScopeAccepted scopeAccepted, CandidatesCallback callback);
    void refresh(QObject* receiver, const DirectMediaScope& scope, ScopeAccepted scopeAccepted,
        RefreshCallback callback, RefreshCallback changesCallback = {});
    void open(QObject* receiver, const DirectMediaScope& scope,
        DirectMediaNavigationOpenRequest request, ScopeAccepted scopeAccepted,
        OpenCallback callback);
    void cancel();

private:
    void startLoad(QObject* receiver, const DirectMediaScope& scope, ScopeAccepted scopeAccepted,
        CandidatesCallback callback);
    void startCandidateChanges(QObject* receiver, const DirectMediaScope& scope,
        ScopeAccepted scopeAccepted, CandidatesCallback callback);
    void finish(const DocumentSessionDirectMediaNavigationLoad& load,
        DocumentSessionDirectMediaNavigationCandidatesResult result,
        const ScopeAccepted& scopeAccepted, const CandidatesCallback& callback);

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DirectMediaNavigationCandidateProvider m_provider;
    ImageIoJob m_job;
    ImageIoJob m_candidateChangesJob;
    quint64 m_candidateChangesRevision = 0;
    DocumentSessionDirectMediaNavigationLoadState m_loadState;
};
}

#endif
