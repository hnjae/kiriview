// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessiondirectmedianavigationcoordinator.h"

#include "navigation/navigationlogging.h"

#include <QDebug>
#include <utility>

namespace kiriview {
namespace {
    void logDirectMediaScope(const char* message, const DirectMediaScope& scope)
    {
        qCDebug(kiriviewNavigationLog)
            << message << "currentUrl" << scope.currentUrl() << "parentUrl" << scope.parentUrl()
            << "generation" << scope.generation();
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
}

DocumentSessionDirectMediaNavigationCoordinator::DocumentSessionDirectMediaNavigationCoordinator(
    DirectMediaNavigationCandidateProvider provider,
    DocumentSessionDirectMediaNavigationCoordinatorPorts ports)
    : m_ports(std::move(ports))
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
        const QUrl cursorUrl = ports.activeCursorUrl ? ports.activeCursorUrl() : QUrl();
        if (!current()) {
            return;
        }
        qCDebug(kiriviewNavigationLog) << "direct media navigation refresh skipped"
                                       << "reason"
                                       << "inactive"
                                       << "cursorUrl" << cursorUrl;
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
    logDirectMediaScope("direct media navigation refresh requested", *scope);
    navigationRuntime->refresh(
        receiver, *scope,
        [control](const DirectMediaScope&) { return !control.isCurrent || control.isCurrent(); },
        [applicationRuntime, activeNavigationSourceKind = ports.activeNavigationSourceKind,
            activeNavigationSnapshot = ports.activeNavigationSnapshot, current,
            control](DocumentSessionDirectMediaNavigationRefreshResult result) {
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
            const ActiveNavigationSourceKind sourceKind = activeNavigationSourceKind
                ? activeNavigationSourceKind()
                : ActiveNavigationSourceKind::None;
            if (!accepted()) {
                return;
            }
            const ActiveNavigationSnapshot snapshot = activeNavigationSnapshot
                ? activeNavigationSnapshot()
                : ActiveNavigationSnapshot {};
            if (!accepted()) {
                return;
            }
            applicationRuntime->applyRefresh(sourceKind, snapshot, std::move(result), control);
        });
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
    const std::shared_ptr<ImageAsyncTicket> admission = m_applicationAdmission;
    DocumentSessionDirectMediaNavigationRuntime* const navigationRuntime = &m_navigationRuntime;
    DocumentSessionDirectMediaNavigationApplicationRuntime* const applicationRuntime
        = &m_applicationRuntime;
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
    navigationRuntime->open(
        receiver, *scope, request,
        [control](const DirectMediaScope&) { return !control.isCurrent || control.isCurrent(); },
        [applicationRuntime, activeCursorUrl = ports.activeCursorUrl, current, control](
            DocumentSessionDirectMediaNavigationOpenResult result) {
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
            const QUrl cursorUrl = activeCursorUrl ? activeCursorUrl() : QUrl();
            if (!accepted()) {
                return;
            }
            applicationRuntime->applyOpen(cursorUrl, std::move(result), control);
        });
}
}
