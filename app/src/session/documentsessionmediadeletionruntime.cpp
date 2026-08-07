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

DocumentSessionMediaDeletionRuntime::~DocumentSessionMediaDeletionRuntime()
{
    m_lifetime.reset();
    cancel();
}

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

    const std::weak_ptr<void> lifetime = m_lifetime;
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    const quint64 operationId = operationState->start();
    ImageIoJob previousJob = std::move(m_job);
    m_candidateRuntime.cancel();
    previousJob.cancel();
    if (lifetime.expired() || !operationState->accepts(operationId)) {
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

    const std::weak_ptr<void> lifetime = m_lifetime;
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    const quint64 operationId = operationState->start();
    ImageIoJob previousJob = std::move(m_job);
    previousJob.cancel();
    if (lifetime.expired() || !operationState->accepts(operationId)) {
        return false;
    }
    auto sharedCallback = std::make_shared<CompletionCallback>(std::move(callback));
    m_candidateRuntime.loadCandidates(receiver, scope, std::move(scopeAccepted),
        [this, lifetime, operationState, receiver, operationId, mode,
            actualTargetUrl = scope.currentUrl(), navigationIdentityUrl = scope.navigationUrl(),
            candidateTargetUrl = scope.parentUrl(), documentKind,
            sharedCallback](DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
            if (lifetime.expired() || !operationState->accepts(operationId)) {
                return;
            }
            if (!result.succeeded) {
                if (!operationState->finish(operationId)) {
                    return;
                }
                KioOperationFailure failure = result.failure.has_value()
                    ? std::move(*result.failure)
                    : kioOperationValidationFailure(KioOperationKind::DirectoryListing,
                          candidateTargetUrl,
                          QStringLiteral("candidate listing failed without typed failure"));
                const bool reportFailure = !failure.canceled;
                invokeIfSet(*sharedCallback,
                    DocumentSessionMediaDeletionCompletion {
                        DocumentSessionMediaDeletionCompletionPlan { {}, reportFailure },
                        std::move(failure),
                    });
                return;
            }
            DocumentSessionMediaDeletionStartPlan plan = documentSessionMediaDeletionStartPlan(
                mode, std::move(result.candidates), actualTargetUrl, navigationIdentityUrl);
            if (!plan.shouldStartDeletion) {
                static_cast<void>(operationState->finish(operationId));
                return;
            }
            static_cast<void>(startFileOperation(
                receiver, operationId, std::move(plan), documentKind, sharedCallback));
        });
    return true;
}

void DocumentSessionMediaDeletionRuntime::cancel()
{
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    ImageIoJob job = std::move(m_job);
    operationState->cancel();
    m_candidateRuntime.cancel();
    job.cancel();
}

bool DocumentSessionMediaDeletionRuntime::active() const { return m_operation->active(); }

bool DocumentSessionMediaDeletionRuntime::startFileOperation(QObject* receiver, quint64 operationId,
    DocumentSessionMediaDeletionStartPlan plan, DocumentSessionKind documentKind,
    const std::shared_ptr<CompletionCallback>& callback)
{
    const std::weak_ptr<void> lifetime = m_lifetime;
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    const FileDeletionProvider provider = m_fileDeletionProvider;
    if (!operationState->accepts(operationId)) {
        return false;
    }

    ImageIoJob startedJob = provider(receiver, plan.request,
        [operationState, operationId, documentKind, fallbackPlan = std::move(plan.fallbackPlan),
            callback](FileDeletionResult result, const KioOperationFailure& failure) {
            if (!operationState->finish(operationId)) {
                return;
            }

            invokeIfSet(*callback,
                DocumentSessionMediaDeletionCompletion {
                    documentSessionMediaDeletionCompletionPlan(documentKind, fallbackPlan, result),
                    failure,
                });
        });
    if (lifetime.expired() || !operationState->accepts(operationId)) {
        startedJob.cancel();
        return true;
    }
    m_job = std::move(startedJob);
    return true;
}
}
