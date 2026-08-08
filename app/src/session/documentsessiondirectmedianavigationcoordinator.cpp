// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessiondirectmedianavigationcoordinator.h"

#include "diagnostics/diagnosticlogprojection.h"
#include "navigation/navigationlogging.h"

#include <QDebug>
#include <QPointer>
#include <mutex>
#include <utility>

namespace kiriview {
namespace {
    void logDirectMediaScope(const char* message, const DirectMediaScope& scope)
    {
        qCDebug(kiriviewNavigationLog)
            << message << "currentUrl" << diagnosticSourceReference(scope.currentUrl())
            << "parentUrl" << diagnosticSourceReference(scope.parentUrl()) << "generation"
            << scope.generation();
    }

    DocumentSessionDirectMediaNavigationApplicationPorts applicationPorts(
        const DocumentSessionDirectMediaNavigationCoordinatorPorts& ports)
    {
        return DocumentSessionDirectMediaNavigationApplicationPorts {
            ports.setDirectMediaNavigation,
            ports.applyRevealAction,
            ports.recomputePublicProjection,
            ports.schedulePredecode,
            ports.openMediaUrl,
        };
    }

    DocumentSessionDirectMediaNavigationApplicationControl applicationControl(
        const std::weak_ptr<void>& lifetime, const std::shared_ptr<ImageAsyncTicket>& admission,
        quint64 applicationRevision, std::optional<DirectMediaScope> scope,
        std::function<bool(const DirectMediaScope&)> cursorMatches,
        std::function<bool()> transitionCurrent)
    {
        const std::function<bool()> applicationCurrent = [lifetime, admission,
                                                             applicationRevision]() {
            return !lifetime.expired() && admission && admission->accepts(applicationRevision);
        };
        return DocumentSessionDirectMediaNavigationApplicationControl {
            [applicationCurrent, scope = std::move(scope), cursorMatches = std::move(cursorMatches),
                transitionCurrent = std::move(transitionCurrent)]() {
                if (!applicationCurrent()) {
                    return false;
                }
                const bool transitionAccepted = !transitionCurrent || transitionCurrent();
                if (!applicationCurrent()) {
                    return false;
                }
                if (!transitionAccepted) {
                    return false;
                }
                const bool scopeCurrent
                    = !scope.has_value() || !cursorMatches || cursorMatches(*scope);
                const bool stillAdmitted = applicationCurrent();
                return scopeCurrent && stillAdmitted;
            },
            applicationCurrent,
        };
    }

    bool applicationControlCurrent(
        const DocumentSessionDirectMediaNavigationApplicationControl& control)
    {
        return !control.isCurrent || control.isCurrent();
    }

    bool confirmedDirectMediaSelection(
        ActiveNavigationSourceKind sourceKind, const ActiveNavigationSnapshot& snapshot)
    {
        return sourceKind == ActiveNavigationSourceKind::OrdinaryDirectMedia && snapshot.known
            && snapshot.currentNumber > 0 && snapshot.currentNumber <= snapshot.count;
    }

    void recoverRemovedCurrentCandidate(const std::weak_ptr<void>& lifetime,
        const std::shared_ptr<ImageAsyncTicket>& admission,
        DocumentSessionDirectMediaNavigationRuntime* navigationRuntime,
        const DocumentSessionDirectMediaNavigationCoordinatorPorts& ports,
        const DirectMediaScope& scope, const std::function<bool()>& transitionCurrent,
        const std::vector<DirectMediaNavigationCandidate>& candidates)
    {
        const QUrl currentUrl
            = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl();
        const std::optional<QUrl> fallbackUrl
            = directMediaNavigationRemovalFallbackUrl(candidates, currentUrl);
        const quint64 recoveryRevision = admission->next();
        const std::function<bool()> recoveryContinuationCurrent
            = [lifetime, admission, recoveryRevision]() {
                  return !lifetime.expired() && admission->accepts(recoveryRevision);
              };
        navigationRuntime->cancel();
        if (!recoveryContinuationCurrent()) {
            return;
        }
        const bool transitionAccepted = !transitionCurrent || transitionCurrent();
        if (!recoveryContinuationCurrent() || !transitionAccepted) {
            return;
        }
        const bool scopeAccepted = !ports.cursorMatches || ports.cursorMatches(scope);
        if (!recoveryContinuationCurrent() || !scopeAccepted) {
            return;
        }

        qCDebug(kiriviewNavigationLog)
            << "direct media navigation current candidate removed"
            << "currentUrl" << diagnosticSourceReference(currentUrl) << "fallbackUrl"
            << diagnosticSourceReference(fallbackUrl.value_or(QUrl()));
        if (ports.recoverRemovedDirectMedia) {
            ports.recoverRemovedDirectMedia(
                fallbackUrl, std::function<bool()>(recoveryContinuationCurrent));
        }
    }
}

class DocumentSessionDirectMediaNavigationCoordinator::CurrentCandidateConfirmation final
{
public:
    void align(const DirectMediaScope& scope, bool confirmed)
    {
        const std::scoped_lock lock(m_mutex);
        if (m_scope.has_value() && *m_scope != scope) {
            m_scope.reset();
        }
        if (confirmed) {
            m_scope = scope;
        }
    }

    void confirm(const DirectMediaScope& scope)
    {
        const std::scoped_lock lock(m_mutex);
        m_scope = scope;
    }

    [[nodiscard]] bool confirmed(const DirectMediaScope& scope) const
    {
        const std::scoped_lock lock(m_mutex);
        return m_scope.has_value() && *m_scope == scope;
    }

    void clear(const DirectMediaScope& scope)
    {
        const std::scoped_lock lock(m_mutex);
        if (m_scope.has_value() && *m_scope == scope) {
            m_scope.reset();
        }
    }

    void clearAll()
    {
        const std::scoped_lock lock(m_mutex);
        m_scope.reset();
    }

private:
    mutable std::mutex m_mutex;
    std::optional<DirectMediaScope> m_scope;
};

DocumentSessionDirectMediaNavigationCoordinator::DocumentSessionDirectMediaNavigationCoordinator(
    DirectMediaNavigationCandidateProvider provider,
    DocumentSessionDirectMediaNavigationCoordinatorPorts ports)
    : m_currentCandidateConfirmation(std::make_shared<CurrentCandidateConfirmation>())
    , m_ports(std::move(ports))
    , m_navigationRuntime(std::move(provider))
    , m_applicationRuntime(applicationPorts(m_ports))
{
}

DocumentSessionDirectMediaNavigationCoordinator::~DocumentSessionDirectMediaNavigationCoordinator()
{
    m_callbackLifetime.reset();
    cancel();
}

void DocumentSessionDirectMediaNavigationCoordinator::cancel()
{
    static_cast<void>(cancelAndCaptureCurrent());
}

std::function<bool()> DocumentSessionDirectMediaNavigationCoordinator::cancelAndCaptureCurrent()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const std::shared_ptr<ImageAsyncTicket> admission = m_applicationAdmission;
    DocumentSessionDirectMediaNavigationRuntime* const navigationRuntime = &m_navigationRuntime;
    const quint64 applicationRevision = admission->next();
    const std::function<bool()> current = [lifetime, admission, applicationRevision]() {
        return !lifetime.expired() && admission->accepts(applicationRevision);
    };
    m_currentCandidateConfirmation->clearAll();
    navigationRuntime->cancel();
    return current;
}

void DocumentSessionDirectMediaNavigationCoordinator::refresh(QObject* receiver)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const std::shared_ptr<ImageAsyncTicket> admission = m_applicationAdmission;
    DocumentSessionDirectMediaNavigationRuntime* const navigationRuntime = &m_navigationRuntime;
    DocumentSessionDirectMediaNavigationApplicationRuntime* const applicationRuntime
        = &m_applicationRuntime;
    const std::shared_ptr<CurrentCandidateConfirmation> currentCandidateConfirmation
        = m_currentCandidateConfirmation;
    const DocumentSessionDirectMediaNavigationCoordinatorPorts ports = m_ports;
    const quint64 applicationRevision = admission->next();
    const auto current = [lifetime, admission, applicationRevision]() {
        return !lifetime.expired() && admission->accepts(applicationRevision);
    };
    if (!current()) {
        return;
    }

    const bool active = ports.navigationActive && ports.navigationActive();
    if (!current()) {
        return;
    }
    if (!active) {
        currentCandidateConfirmation->clearAll();
        navigationRuntime->cancel();
        if (!current()) {
            return;
        }
        const QUrl cursorUrl = ports.activeCursorUrl ? ports.activeCursorUrl() : QUrl();
        if (!current()) {
            return;
        }
        qCDebug(kiriviewNavigationLog) << "direct media navigation refresh skipped"
                                       << "reason"
                                       << "inactive"
                                       << "cursorUrl" << diagnosticSourceReference(cursorUrl);
        const std::function<bool()> transitionCurrent = ports.captureRefreshTransitionCurrent
            ? ports.captureRefreshTransitionCurrent()
            : std::function<bool()> {};
        if (!current()) {
            return;
        }
        const DocumentSessionDirectMediaNavigationApplicationControl control
            = applicationControl(lifetime, admission, applicationRevision, std::nullopt,
                ports.cursorMatches, transitionCurrent);
        const bool controlCurrent = applicationControlCurrent(control);
        if (!current() || !controlCurrent) {
            return;
        }
        applicationRuntime->applyInactiveRefresh(control);
        return;
    }

    const std::optional<DirectMediaScope> scope
        = ports.currentScope ? ports.currentScope() : std::nullopt;
    if (!current()) {
        return;
    }
    if (!scope.has_value()) {
        currentCandidateConfirmation->clearAll();
        navigationRuntime->cancel();
        if (!current()) {
            return;
        }
        const std::function<bool()> transitionCurrent = ports.captureRefreshTransitionCurrent
            ? ports.captureRefreshTransitionCurrent()
            : std::function<bool()> {};
        if (!current()) {
            return;
        }
        const DocumentSessionDirectMediaNavigationApplicationControl control
            = applicationControl(lifetime, admission, applicationRevision, std::nullopt,
                ports.cursorMatches, transitionCurrent);
        const bool controlCurrent = applicationControlCurrent(control);
        if (!current() || !controlCurrent) {
            return;
        }
        applicationRuntime->applyInactiveRefresh(control);
        return;
    }

    const std::function<bool()> transitionCurrent = ports.captureRefreshTransitionCurrent
        ? ports.captureRefreshTransitionCurrent()
        : std::function<bool()> {};
    if (!current()) {
        return;
    }
    const DocumentSessionDirectMediaNavigationApplicationControl control = applicationControl(
        lifetime, admission, applicationRevision, scope, ports.cursorMatches, transitionCurrent);
    const bool controlCurrent = applicationControlCurrent(control);
    if (!current() || !controlCurrent) {
        return;
    }
    const ActiveNavigationSourceKind sourceKindBeforeRefresh = ports.activeNavigationSourceKind
        ? ports.activeNavigationSourceKind()
        : ActiveNavigationSourceKind::None;
    if (!current()) {
        return;
    }
    const ActiveNavigationSnapshot snapshotBeforeRefresh = ports.activeNavigationSnapshot
        ? ports.activeNavigationSnapshot()
        : ActiveNavigationSnapshot {};
    if (!current()) {
        return;
    }
    currentCandidateConfirmation->align(
        *scope, confirmedDirectMediaSelection(sourceKindBeforeRefresh, snapshotBeforeRefresh));
    logDirectMediaScope("direct media navigation refresh requested", *scope);
    const auto applyRefreshResult
        = [lifetime, admission, navigationRuntime, applicationRuntime, ports, current, control,
              scope = *scope, currentCandidateConfirmation,
              transitionCurrent](DocumentSessionDirectMediaNavigationRefreshResult result) mutable {
              const auto accepted = [&]() {
                  if (!current()) {
                      return false;
                  }
                  const bool controlCurrent = applicationControlCurrent(control);
                  return current() && controlCurrent;
              };
              if (!accepted()) {
                  return;
              }

              const QUrl currentUrl
                  = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl();
              const bool currentCandidatePresent = result.succeeded
                  && directMediaNavigationCandidateIndex(result.candidates, currentUrl).has_value();
              if (currentCandidatePresent) {
                  currentCandidateConfirmation->confirm(scope);
              } else if (result.succeeded && currentCandidateConfirmation->confirmed(scope)) {
                  if (!accepted()) {
                      return;
                  }
                  currentCandidateConfirmation->clear(scope);
                  recoverRemovedCurrentCandidate(lifetime, admission, navigationRuntime, ports,
                      scope, transitionCurrent, result.candidates);
                  return;
              }

              const ActiveNavigationSourceKind sourceKind = ports.activeNavigationSourceKind
                  ? ports.activeNavigationSourceKind()
                  : ActiveNavigationSourceKind::None;
              if (!accepted()) {
                  return;
              }
              const ActiveNavigationSnapshot snapshot = ports.activeNavigationSnapshot
                  ? ports.activeNavigationSnapshot()
                  : ActiveNavigationSnapshot {};
              if (!accepted()) {
                  return;
              }
              applicationRuntime->applyRefresh(sourceKind, snapshot, std::move(result), control);
          };
    navigationRuntime->refresh(
        receiver, *scope,
        [control](const DirectMediaScope&) { return !control.isCurrent || control.isCurrent(); },
        applyRefreshResult, applyRefreshResult);
}

void DocumentSessionDirectMediaNavigationCoordinator::openPrevious(QObject* receiver)
{
    open(receiver, previousDirectMediaNavigationOpenRequest());
}

void DocumentSessionDirectMediaNavigationCoordinator::openNext(QObject* receiver)
{
    open(receiver, nextDirectMediaNavigationOpenRequest());
}

void DocumentSessionDirectMediaNavigationCoordinator::openAtNumber(
    QObject* receiver, int mediaNumber)
{
    open(receiver, numberedDirectMediaNavigationOpenRequest(mediaNumber));
}

void DocumentSessionDirectMediaNavigationCoordinator::open(
    QObject* receiver, DirectMediaNavigationOpenRequest request)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const QPointer<QObject> guardedReceiver(receiver);
    const bool receiverWasProvided = receiver != nullptr;
    const std::shared_ptr<ImageAsyncTicket> admission = m_applicationAdmission;
    DocumentSessionDirectMediaNavigationCoordinator* const coordinator = this;
    DocumentSessionDirectMediaNavigationRuntime* const navigationRuntime = &m_navigationRuntime;
    DocumentSessionDirectMediaNavigationApplicationRuntime* const applicationRuntime
        = &m_applicationRuntime;
    const std::shared_ptr<CurrentCandidateConfirmation> currentCandidateConfirmation
        = m_currentCandidateConfirmation;
    const DocumentSessionDirectMediaNavigationCoordinatorPorts ports = m_ports;
    const quint64 applicationRevision = admission->next();
    const auto current = [lifetime, admission, applicationRevision]() {
        return !lifetime.expired() && admission->accepts(applicationRevision);
    };
    if (!current()) {
        return;
    }

    const bool active = ports.navigationActive && ports.navigationActive();
    if (!current() || !active) {
        return;
    }

    const std::optional<DirectMediaScope> scope
        = ports.currentScope ? ports.currentScope() : std::nullopt;
    if (!current()) {
        return;
    }
    if (!scope.has_value()) {
        return;
    }

    const std::function<bool()> transitionCurrent = ports.captureOpenTransitionCurrent
        ? ports.captureOpenTransitionCurrent()
        : std::function<bool()> {};
    if (!current()) {
        return;
    }
    const DocumentSessionDirectMediaNavigationApplicationControl control = applicationControl(
        lifetime, admission, applicationRevision, scope, ports.cursorMatches, transitionCurrent);
    const bool controlCurrent = applicationControlCurrent(control);
    if (!current() || !controlCurrent) {
        return;
    }
    const ActiveNavigationSourceKind sourceKindBeforeOpen = ports.activeNavigationSourceKind
        ? ports.activeNavigationSourceKind()
        : ActiveNavigationSourceKind::None;
    if (!current()) {
        return;
    }
    const ActiveNavigationSnapshot snapshotBeforeOpen = ports.activeNavigationSnapshot
        ? ports.activeNavigationSnapshot()
        : ActiveNavigationSnapshot {};
    if (!current()) {
        return;
    }
    currentCandidateConfirmation->align(
        *scope, confirmedDirectMediaSelection(sourceKindBeforeOpen, snapshotBeforeOpen));
    navigationRuntime->cancel();
    if (!current() || !applicationControlCurrent(control)) {
        return;
    }
    navigationRuntime->open(
        receiver, *scope, request,
        [control](const DirectMediaScope&) { return !control.isCurrent || control.isCurrent(); },
        [applicationRuntime, activeCursorUrl = ports.activeCursorUrl, coordinator, lifetime,
            guardedReceiver, receiverWasProvided, current, control, admission, navigationRuntime,
            ports, scope = *scope, transitionCurrent,
            currentCandidateConfirmation](DocumentSessionDirectMediaNavigationOpenResult result) {
            const auto accepted = [&]() {
                if (!current()) {
                    return false;
                }
                const bool controlCurrent = applicationControlCurrent(control);
                return current() && controlCurrent;
            };
            if (!accepted()) {
                return;
            }
            const QUrl scopedCurrentUrl
                = scope.navigationUrl().isEmpty() ? scope.currentUrl() : scope.navigationUrl();
            const bool currentCandidatePresent = result.succeeded
                && directMediaNavigationCandidateIndex(result.candidates, scopedCurrentUrl)
                       .has_value();
            if (currentCandidatePresent) {
                currentCandidateConfirmation->confirm(scope);
            } else if (result.succeeded && currentCandidateConfirmation->confirmed(scope)) {
                if (!accepted()) {
                    return;
                }
                currentCandidateConfirmation->clear(scope);
                recoverRemovedCurrentCandidate(lifetime, admission, navigationRuntime, ports, scope,
                    transitionCurrent, result.candidates);
                return;
            }
            const bool routeExpected = currentCandidatePresent && result.plan.targetUrl.has_value();
            const QUrl cursorUrl = activeCursorUrl ? activeCursorUrl() : QUrl();
            if (!accepted()) {
                return;
            }
            applicationRuntime->applyOpen(cursorUrl, std::move(result), control);
            if (routeExpected || !accepted() || (receiverWasProvided && guardedReceiver.isNull())
                || lifetime.expired()) {
                return;
            }
            coordinator->refresh(guardedReceiver.data());
        });
}
}
