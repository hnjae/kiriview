// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationruntime.h"

#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "location/imageurl.h"
#include "navigation/navigationlogging.h"
#include "system/kiooperationfailure.h"

#include <QDebug>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace {
bool sameDirectMediaNavigationCandidates(
    const std::vector<kiriview::DirectMediaNavigationCandidate>& left,
    const std::vector<kiriview::DirectMediaNavigationCandidate>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!kiriview::sameNormalizedUrl(left[index].url, right[index].url)
            || left[index].name != right[index].name
            || left[index].sourceFreshness != right[index].sourceFreshness) {
            return false;
        }
    }
    return true;
}

class DirectMediaNavigationRefreshStreamState final
{
public:
    bool accepts(const kiriview::DocumentSessionDirectMediaNavigationCandidatesResult& result,
        bool candidateChange)
    {
        const std::scoped_lock lock(m_mutex);
        if (candidateChange) {
            m_changeDelivered = true;
        } else if (m_changeDelivered) {
            return false;
        }
        if (!result.succeeded) {
            if (m_failureDelivered) {
                return false;
            }
            m_failureDelivered = true;
            m_lastCandidates.reset();
            return true;
        }
        m_failureDelivered = false;
        if (m_lastCandidates.has_value()
            && sameDirectMediaNavigationCandidates(*m_lastCandidates, result.candidates)) {
            return false;
        }
        m_lastCandidates = result.candidates;
        return true;
    }

private:
    std::mutex m_mutex;
    bool m_changeDelivered = false;
    bool m_failureDelivered = false;
    std::optional<std::vector<kiriview::DirectMediaNavigationCandidate>> m_lastCandidates;
};

kiriview::DocumentSessionDirectMediaNavigationRefreshResult directMediaNavigationRefreshResult(
    kiriview::DocumentSessionDirectMediaNavigationCandidatesResult result, const QUrl& currentUrl)
{
    if (!result.succeeded) {
        return kiriview::DocumentSessionDirectMediaNavigationRefreshResult { std::move(
                                                                                 result.candidates),
            {}, false, std::move(result.errorString), std::move(result.failure) };
    }

    const kiriview::DirectMediaNavigationBoundaryState boundaryState
        = kiriview::directMediaNavigationBoundaryState(result.candidates, currentUrl);
    return kiriview::DocumentSessionDirectMediaNavigationRefreshResult { std::move(
                                                                             result.candidates),
        boundaryState, true, std::move(result.errorString), std::move(result.failure) };
}

kiriview::DocumentSessionDirectMediaNavigationCandidatesResult
validatedDirectMediaNavigationCandidatesResult(
    kiriview::DocumentSessionDirectMediaNavigationCandidatesResult result,
    const kiriview::DirectMediaScope& scope)
{
    if (!result.succeeded
        || kiriview::directMediaNavigationCandidatesBelongToScope(
            result.candidates, scope.parentUrl())) {
        return result;
    }

    kiriview::KioOperationFailure failure = kiriview::kioOperationValidationFailure(
        kiriview::KioOperationKind::DirectoryListing, scope.parentUrl(),
        QStringLiteral(
            "directory candidate provider returned a candidate outside the requested scope"));
    return kiriview::DocumentSessionDirectMediaNavigationCandidatesResult { {}, false,
        failure.userMessage, std::move(failure) };
}
}

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
    const DirectMediaScope& scope, ScopeAccepted scopeAccepted, RefreshCallback callback,
    RefreshCallback changesCallback)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const QUrl currentUrl
        = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl();
    auto streamState = std::make_shared<DirectMediaNavigationRefreshStreamState>();
    const quint64 expectedCandidateChangesRevision = m_candidateChangesRevision + 1;
    if (changesCallback) {
        startCandidateChanges(receiver, scope, scopeAccepted,
            [streamState, changesCallback = std::move(changesCallback), currentUrl](
                DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
                if (!streamState->accepts(result, true)) {
                    return;
                }
                invokeIfSet(changesCallback,
                    directMediaNavigationRefreshResult(std::move(result), currentUrl));
            });
    } else {
        startCandidateChanges(receiver, scope, scopeAccepted, {});
    }
    if (lifetime.expired() || m_candidateChangesRevision != expectedCandidateChangesRevision) {
        return;
    }
    startLoad(receiver, scope, std::move(scopeAccepted),
        [streamState, callback = std::move(callback), currentUrl](
            DocumentSessionDirectMediaNavigationCandidatesResult result) mutable {
            if (!streamState->accepts(result, false)) {
                return;
            }
            invokeIfSet(
                callback, directMediaNavigationRefreshResult(std::move(result), currentUrl));
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
                    DocumentSessionDirectMediaNavigationOpenResult { std::move(result.candidates),
                        {}, false, std::move(result.errorString), std::move(result.failure) });
                return;
            }

            DirectMediaNavigationOpenPlan plan
                = directMediaNavigationOpenPlan(result.candidates, currentUrl, request);
            invokeIfSet(callback,
                DocumentSessionDirectMediaNavigationOpenResult { std::move(result.candidates),
                    std::move(plan), true, std::move(result.errorString),
                    std::move(result.failure) });
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
                    std::move(candidates), true, QString(), std::nullopt },
                *sharedScopeAccepted, *sharedCallback);
        },
        [this, lifetime, load, sharedScopeAccepted, sharedCallback](KioOperationFailure failure) {
            if (lifetime.expired()) {
                return;
            }
            QString errorString = failure.userMessage;
            finish(load,
                DocumentSessionDirectMediaNavigationCandidatesResult {
                    {}, false, std::move(errorString), std::move(failure) },
                *sharedScopeAccepted, *sharedCallback);
        });
    if (lifetime.expired() || !m_loadState.accepts(load)) {
        startedJob.cancel();
        return;
    }
    m_job = std::move(startedJob);
}

void DocumentSessionDirectMediaNavigationRuntime::startCandidateChanges(QObject* receiver,
    const DirectMediaScope& scope, ScopeAccepted scopeAccepted, CandidatesCallback callback)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DirectMediaNavigationCandidateProvider provider = m_provider;
    const quint64 revision = ++m_candidateChangesRevision;
    ImageIoJob previousJob = std::move(m_candidateChangesJob);
    previousJob.cancel();
    if (lifetime.expired() || m_candidateChangesRevision != revision) {
        return;
    }

    if (scope.currentUrl().isEmpty() || scope.parentUrl().isEmpty() || !scope.parentUrl().isValid()
        || !scope.parentUrl().isLocalFile() || !provider.directoryCandidateChanges || !callback) {
        return;
    }

    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate watch started"
        << "revision" << revision << "currentUrl" << diagnosticSourceReference(scope.currentUrl())
        << "parentUrl" << diagnosticSourceReference(scope.parentUrl()) << "generation"
        << scope.generation();
    auto sharedScopeAccepted = std::make_shared<ScopeAccepted>(std::move(scopeAccepted));
    auto sharedCallback = std::make_shared<CandidatesCallback>(std::move(callback));
    ImageIoJob startedJob = provider.directoryCandidateChanges(
        receiver, scope.parentUrl(),
        [this, lifetime, revision, scope, sharedScopeAccepted, sharedCallback](
            std::vector<DirectMediaNavigationCandidate> candidates) mutable {
            if (lifetime.expired() || m_candidateChangesRevision != revision) {
                return;
            }
            const bool scopeIsAccepted = !*sharedScopeAccepted || (*sharedScopeAccepted)(scope);
            if (lifetime.expired() || m_candidateChangesRevision != revision) {
                return;
            }
            if (!scopeIsAccepted) {
                cancel();
                return;
            }

            qCDebug(kiriviewNavigationLog)
                << "direct media navigation candidate watch changed"
                << "revision" << revision << "currentUrl"
                << diagnosticSourceReference(scope.currentUrl()) << "parentUrl"
                << diagnosticSourceReference(scope.parentUrl()) << "generation"
                << scope.generation() << "candidates" << candidates.size();
            invokeIfSet(*sharedCallback,
                validatedDirectMediaNavigationCandidatesResult(
                    DocumentSessionDirectMediaNavigationCandidatesResult {
                        std::move(candidates), true, QString(), std::nullopt },
                    scope));
        },
        [this, lifetime, revision, scope, sharedScopeAccepted, sharedCallback](
            const KioOperationFailure& failure) {
            if (lifetime.expired() || m_candidateChangesRevision != revision) {
                return;
            }
            const bool scopeIsAccepted = !*sharedScopeAccepted || (*sharedScopeAccepted)(scope);
            if (lifetime.expired() || m_candidateChangesRevision != revision || !scopeIsAccepted) {
                return;
            }

            qCWarning(kiriviewNavigationLog)
                << "direct media navigation candidate watch failed"
                << "revision" << revision << "currentUrl"
                << diagnosticSourceReference(scope.currentUrl()) << "parentUrl"
                << diagnosticSourceReference(scope.parentUrl()) << "generation"
                << scope.generation() << "error"
                << diagnosticDetailReference(failure.diagnosticDetail);
            invokeIfSet(*sharedCallback,
                DocumentSessionDirectMediaNavigationCandidatesResult {
                    {}, false, failure.userMessage, failure });
        });
    if (lifetime.expired() || m_candidateChangesRevision != revision) {
        startedJob.cancel();
        return;
    }
    m_candidateChangesJob = std::move(startedJob);
}

void DocumentSessionDirectMediaNavigationRuntime::cancel()
{
    ImageIoJob job = std::move(m_job);
    ImageIoJob candidateChangesJob = std::move(m_candidateChangesJob);
    m_loadState.cancel();
    ++m_candidateChangesRevision;
    job.cancel();
    candidateChangesJob.cancel();
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

    result = validatedDirectMediaNavigationCandidatesResult(std::move(result), load.scope);

    const QString diagnosticDetail
        = result.failure.has_value() ? result.failure->diagnosticDetail : result.errorString;
    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidate load finished"
        << "operationId" << load.operationId << "succeeded" << result.succeeded << "candidates"
        << result.candidates.size() << "error" << diagnosticDetailReference(diagnosticDetail);
    invokeIfSet(callback, std::move(result));
}
}
