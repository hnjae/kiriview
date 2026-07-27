// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONMEDIADELETIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONMEDIADELETIONRUNTIME_H

#include "async/imageasyncoperationstate.h"
#include "async/imageiojob.h"
#include "navigation/directmedianavigationmodel.h"
#include "session/documentsessiondirectmedianavigationruntime.h"
#include "session/documentsessionmediadeletionplan.h"
#include "session/documentsessiontypes.h"
#include "system/filedeletion.h"

#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <vector>

class QObject;

namespace kiriview {
struct DocumentSessionMediaDeletionCompletion
{
    DocumentSessionMediaDeletionCompletionPlan plan;
    KioOperationFailure failure;
};

class DocumentSessionMediaDeletionRuntime final
{
public:
    using CompletionCallback = std::function<void(DocumentSessionMediaDeletionCompletion)>;
    using ScopeAccepted = DocumentSessionDirectMediaNavigationRuntime::ScopeAccepted;

    explicit DocumentSessionMediaDeletionRuntime(FileDeletionProvider fileDeletionProvider = {},
        DirectMediaNavigationCandidateProvider candidateProvider = {});
    ~DocumentSessionMediaDeletionRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionMediaDeletionRuntime)

    DocumentSessionMediaDeletionStartPlan start(QObject* receiver, FileDeletionMode mode,
        std::vector<DirectMediaNavigationCandidate> candidates, const QUrl& actualTargetUrl,
        const QUrl& navigationIdentityUrl, DocumentSessionKind documentKind,
        CompletionCallback callback);
    bool startForDirectMedia(QObject* receiver, FileDeletionMode mode,
        const DirectMediaScope& scope, ScopeAccepted scopeAccepted,
        DocumentSessionKind documentKind, CompletionCallback callback);
    void cancel();
    [[nodiscard]] bool active() const;

private:
    bool startFileOperation(QObject* receiver, quint64 operationId,
        DocumentSessionMediaDeletionStartPlan plan, DocumentSessionKind documentKind,
        const std::shared_ptr<CompletionCallback>& callback);

    std::shared_ptr<void> m_lifetime = std::make_shared<char>();
    FileDeletionProvider m_fileDeletionProvider;
    DocumentSessionDirectMediaNavigationRuntime m_candidateRuntime;
    ImageIoJob m_job;
    std::shared_ptr<ImageAsyncOperationState> m_operation {
        std::make_shared<ImageAsyncOperationState>()
    };
};
}

#endif
