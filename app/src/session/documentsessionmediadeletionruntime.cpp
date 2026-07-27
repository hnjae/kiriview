// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionmediadeletionruntime.h"

#include "async/imagecallback.h"

#include <memory>
#include <utility>

namespace kiriview {
DocumentSessionMediaDeletionRuntime::DocumentSessionMediaDeletionRuntime(
    FileDeletionProvider fileDeletionProvider,
    DirectMediaNavigationCandidateProvider candidateProvider)
    : m_fileDeletionProvider(fileDeletionProviderWithDefault(std::move(fileDeletionProvider)))
    , m_candidateRuntime(std::move(candidateProvider))
{
}

DocumentSessionMediaDeletionRuntime::~DocumentSessionMediaDeletionRuntime() { cancel(); }

DocumentSessionMediaDeletionStartPlan DocumentSessionMediaDeletionRuntime::start(QObject* receiver,
    FileDeletionMode mode, std::vector<DirectMediaNavigationCandidate> candidates,
    const QUrl& actualTargetUrl, const QUrl& navigationIdentityUrl,
    DocumentSessionKind documentKind, CompletionCallback callback)
{
    DocumentSessionMediaDeletionStartPlan plan = documentSessionMediaDeletionStartPlan(
        mode, std::move(candidates), actualTargetUrl, navigationIdentityUrl);
    if (!plan.shouldStartDeletion) {
        return plan;
    }

    const quint64 operationId = m_operation.start();
    m_candidateRuntime.cancel();
    m_job.cancel();
    if (!m_operation.accepts(operationId)) {
        return {};
    }
    auto sharedCallback = std::make_shared<CompletionCallback>(std::move(callback));
    if (!startFileOperation(receiver, operationId, plan, documentKind, sharedCallback)) {
        return {};
    }
    return plan;
}

bool DocumentSessionMediaDeletionRuntime::startForDirectMedia(QObject* receiver,
    FileDeletionMode mode, const DirectMediaScope& scope, ScopeAccepted scopeAccepted,
    DocumentSessionKind documentKind, CompletionCallback callback)
{
    if (scope.currentUrl().isEmpty() || scope.parentUrl().isEmpty()
        || !scope.parentUrl().isValid()) {
        return false;
    }

    const quint64 operationId = m_operation.start();
    m_job.cancel();
    if (!m_operation.accepts(operationId)) {
        return false;
    }
    auto sharedCallback = std::make_shared<CompletionCallback>(std::move(callback));
    m_candidateRuntime.loadCandidates(receiver, scope, std::move(scopeAccepted),
        [this, receiver, operationId, mode, actualTargetUrl = scope.currentUrl(),
            navigationIdentityUrl = scope.navigationUrl(), candidateTargetUrl = scope.parentUrl(),
            documentKind,
            sharedCallback](DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
            if (!m_operation.accepts(operationId)) {
                return;
            }
            if (!result.succeeded) {
                if (!m_operation.finish(operationId)) {
                    return;
                }
                invokeIfSet(*sharedCallback,
                    DocumentSessionMediaDeletionCompletion {
                        DocumentSessionMediaDeletionCompletionPlan { {}, true },
                        KioOperationFailure {
                            KioOperationKind::DirectoryListing,
                            candidateTargetUrl,
                            std::nullopt,
                            false,
                            result.errorString,
                            result.errorString,
                            false,
                        },
                    });
                return;
            }
            DocumentSessionMediaDeletionStartPlan plan = documentSessionMediaDeletionStartPlan(
                mode, std::move(result.candidates), actualTargetUrl, navigationIdentityUrl);
            if (!plan.shouldStartDeletion) {
                static_cast<void>(m_operation.finish(operationId));
                return;
            }
            static_cast<void>(startFileOperation(
                receiver, operationId, std::move(plan), documentKind, sharedCallback));
        });
    return true;
}

void DocumentSessionMediaDeletionRuntime::cancel()
{
    m_operation.cancel();
    m_candidateRuntime.cancel();
    m_job.cancel();
}

bool DocumentSessionMediaDeletionRuntime::active() const { return m_operation.active(); }

bool DocumentSessionMediaDeletionRuntime::startFileOperation(QObject* receiver, quint64 operationId,
    DocumentSessionMediaDeletionStartPlan plan, DocumentSessionKind documentKind,
    const std::shared_ptr<CompletionCallback>& callback)
{
    if (!m_operation.accepts(operationId)) {
        return false;
    }

    ImageIoJob startedJob = m_fileDeletionProvider(receiver, plan.request,
        [this, operationId, documentKind, fallbackPlan = std::move(plan.fallbackPlan), callback](
            FileDeletionResult result, const KioOperationFailure& failure) {
            finish(operationId, documentKind, fallbackPlan, result, failure, *callback);
        });
    if (!m_operation.accepts(operationId)) {
        startedJob.cancel();
        return true;
    }
    m_job = std::move(startedJob);
    return true;
}

void DocumentSessionMediaDeletionRuntime::finish(quint64 operationId,
    DocumentSessionKind documentKind, const DocumentSessionMediaDeletionFallbackPlan& fallbackPlan,
    FileDeletionResult result, const KioOperationFailure& failure,
    const CompletionCallback& callback)
{
    if (!m_operation.finish(operationId)) {
        return;
    }

    invokeIfSet(callback,
        DocumentSessionMediaDeletionCompletion {
            documentSessionMediaDeletionCompletionPlan(documentKind, fallbackPlan, result),
            failure,
        });
}
}
