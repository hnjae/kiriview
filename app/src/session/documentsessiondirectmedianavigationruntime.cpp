// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationruntime.h"

#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "navigation/navigationlogging.h"

#include <QDebug>
#include <memory>
#include <utility>

namespace kiriview {
DocumentSessionDirectMediaNavigationRuntime::DocumentSessionDirectMediaNavigationRuntime(
    DirectMediaNavigationCandidateProvider provider)
    : m_provider(directMediaNavigationCandidateProviderWithDefault(std::move(provider)))
{
}

DocumentSessionDirectMediaNavigationRuntime::~DocumentSessionDirectMediaNavigationRuntime()
{
    m_callbackLifetime.reset();
    cancel();
}

void DocumentSessionDirectMediaNavigationRuntime::loadCandidates(QObject* receiver,
    const DirectMediaScope& scope, ScopeAccepted scopeAccepted, CandidatesCallback callback)
{
    startLoad(receiver, scope, std::move(scopeAccepted), std::move(callback));
}

void DocumentSessionDirectMediaNavigationRuntime::refresh(QObject* receiver,
    const DirectMediaScope& scope, ScopeAccepted scopeAccepted, RefreshCallback callback)
{
    startLoad(receiver, scope, std::move(scopeAccepted),
        [callback = std::move(callback),
            currentUrl
            = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl()](
            DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
            if (!result.succeeded) {
                invokeIfSet(callback,
                    DocumentSessionDirectMediaNavigationRefreshResult {
                        std::move(result.candidates), {}, false, result.errorString });
                return;
            }

            const DirectMediaNavigationBoundaryState boundaryState
                = directMediaNavigationBoundaryState(result.candidates, currentUrl);
            invokeIfSet(callback,
                DocumentSessionDirectMediaNavigationRefreshResult {
                    std::move(result.candidates), boundaryState, true, result.errorString });
        });
}

void DocumentSessionDirectMediaNavigationRuntime::open(QObject* receiver,
    const DirectMediaScope& scope, DirectMediaNavigationOpenRequest request,
    ScopeAccepted scopeAccepted, OpenCallback callback)
{
    startLoad(receiver, scope, std::move(scopeAccepted),
        [callback = std::move(callback),
            currentUrl
            = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl(),
            request](DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
            if (!result.succeeded) {
                invokeIfSet(callback,
                    DocumentSessionDirectMediaNavigationOpenResult {
                        std::move(result.candidates), {}, false, result.errorString });
                return;
            }

            DirectMediaNavigationOpenPlan plan
                = directMediaNavigationOpenPlan(result.candidates, currentUrl, request);
            invokeIfSet(callback,
                DocumentSessionDirectMediaNavigationOpenResult {
                    std::move(result.candidates), std::move(plan), true, result.errorString });
        });
}

void DocumentSessionDirectMediaNavigationRuntime::startLoad(QObject* receiver,
    const DirectMediaScope& scope, ScopeAccepted scopeAccepted, CandidatesCallback callback)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DirectMediaNavigationCandidateProvider provider = m_provider;
    if (scope.currentUrl().isEmpty() || scope.parentUrl().isEmpty() || !scope.parentUrl().isValid()
        || !provider.directoryCandidateLoader) {
        cancel();
        if (lifetime.expired()) {
            return;
        }
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation candidate load skipped"
            << "reason"
            << "invalid-scope"
            << "currentUrl" << diagnosticSourceReference(scope.currentUrl()) << "parentUrl"
            << diagnosticSourceReference(scope.parentUrl()) << "generation" << scope.generation()
            << "providerPresent" << static_cast<bool>(provider.directoryCandidateLoader);
        invokeIfSet(callback, DocumentSessionDirectMediaNavigationCandidatesResult {});
        return;
    }

    const DocumentSessionDirectMediaNavigationLoad load = m_loadState.start(scope);
    ImageIoJob previousJob = std::move(m_job);
    previousJob.cancel();
    if (lifetime.expired() || !m_loadState.accepts(load)) {
        return;
    }
    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate load started"
        << "operationId" << load.operationId << "currentUrl"
        << diagnosticSourceReference(scope.currentUrl()) << "parentUrl"
        << diagnosticSourceReference(scope.parentUrl()) << "generation" << scope.generation();
    auto sharedScopeAccepted = std::make_shared<ScopeAccepted>(std::move(scopeAccepted));
    auto sharedCallback = std::make_shared<CandidatesCallback>(std::move(callback));

    ImageIoJob startedJob = provider.directoryCandidateLoader(
        receiver, scope.parentUrl(),
        [this, lifetime, load, sharedScopeAccepted, sharedCallback](
            std::vector<DirectMediaNavigationCandidate> candidates) mutable {
            if (lifetime.expired()) {
                return;
            }
            finish(load,
                DocumentSessionDirectMediaNavigationCandidatesResult {
                    std::move(candidates), true, QString() },
                *sharedScopeAccepted, *sharedCallback);
        },
        [this, lifetime, load, sharedScopeAccepted, sharedCallback](const QString& errorString) {
            if (lifetime.expired()) {
                return;
            }
            finish(load,
                DocumentSessionDirectMediaNavigationCandidatesResult { {}, false, errorString },
                *sharedScopeAccepted, *sharedCallback);
        });
    if (lifetime.expired() || !m_loadState.accepts(load)) {
        startedJob.cancel();
        return;
    }
    m_job = std::move(startedJob);
}

void DocumentSessionDirectMediaNavigationRuntime::cancel()
{
    ImageIoJob job = std::move(m_job);
    m_loadState.cancel();
    job.cancel();
}

void DocumentSessionDirectMediaNavigationRuntime::finish(
    const DocumentSessionDirectMediaNavigationLoad& load,
    DocumentSessionDirectMediaNavigationCandidatesResult result, const ScopeAccepted& scopeAccepted,
    const CandidatesCallback& callback)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (!m_loadState.accepts(load)) {
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation candidate load ignored"
            << "reason"
            << "stale-load"
            << "operationId" << load.operationId << "currentUrl"
            << diagnosticSourceReference(load.scope.currentUrl()) << "parentUrl"
            << diagnosticSourceReference(load.scope.parentUrl()) << "generation"
            << load.scope.generation();
        return;
    }

    const bool scopeIsAccepted = !scopeAccepted || scopeAccepted(load.scope);
    if (lifetime.expired() || !m_loadState.accepts(load)) {
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation candidate load ignored"
            << "reason"
            << "stale-after-scope-check"
            << "operationId" << load.operationId << "currentUrl"
            << diagnosticSourceReference(load.scope.currentUrl()) << "parentUrl"
            << diagnosticSourceReference(load.scope.parentUrl()) << "generation"
            << load.scope.generation();
        return;
    }
    if (!scopeIsAccepted) {
        static_cast<void>(m_loadState.finish(load));
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation candidate load ignored"
            << "reason"
            << "scope-rejected"
            << "operationId" << load.operationId << "currentUrl"
            << diagnosticSourceReference(load.scope.currentUrl()) << "parentUrl"
            << diagnosticSourceReference(load.scope.parentUrl()) << "generation"
            << load.scope.generation();
        return;
    }
    if (!m_loadState.finish(load)) {
        return;
    }

    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate load finished"
        << "operationId" << load.operationId << "succeeded" << result.succeeded << "candidates"
        << result.candidates.size() << "error" << diagnosticDetailReference(result.errorString);
    invokeIfSet(callback, std::move(result));
}
}
