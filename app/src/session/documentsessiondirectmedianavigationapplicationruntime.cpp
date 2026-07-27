// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationapplicationruntime.h"

#include "async/imagecallback.h"
#include "navigation/navigationlogging.h"

#include <QDebug>
#include <utility>

namespace kiriview {
DocumentSessionDirectMediaNavigationApplicationRuntime::
    DocumentSessionDirectMediaNavigationApplicationRuntime(
        DocumentSessionDirectMediaNavigationApplicationPorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionDirectMediaNavigationApplicationRuntime::
    ~DocumentSessionDirectMediaNavigationApplicationRuntime()
{
    m_callbackLifetime.reset();
}

void DocumentSessionDirectMediaNavigationApplicationRuntime::applyInactiveRefresh(
    const DocumentSessionDirectMediaNavigationApplicationControl& control)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionDirectMediaNavigationApplicationPorts ports = m_ports;
    const std::function<bool()> externallyCurrent = control.isCurrent;
    const auto current = [lifetime, externallyCurrent]() {
        if (lifetime.expired()) {
            return false;
        }
        const bool accepted = !externallyCurrent || externallyCurrent();
        return accepted && !lifetime.expired();
    };
    if (!current()) {
        return;
    }

    invokeIfSet(ports.setDirectMediaNavigation, DirectMediaNavigationBoundaryState {}, false,
        std::vector<DirectMediaNavigationCandidate> {});
    if (!current()) {
        return;
    }
    invokeIfSet(ports.applyRevealAction,
        DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync);
    if (!current()) {
        return;
    }
    invokeIfSet(ports.publishProjection);
}

void DocumentSessionDirectMediaNavigationApplicationRuntime::applyRefresh(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot previousSnapshot,
    DocumentSessionDirectMediaNavigationRefreshResult result,
    const DocumentSessionDirectMediaNavigationApplicationControl& control)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionDirectMediaNavigationApplicationPorts ports = m_ports;
    const std::function<bool()> externallyCurrent = control.isCurrent;
    const auto current = [lifetime, externallyCurrent]() {
        if (lifetime.expired()) {
            return false;
        }
        const bool accepted = !externallyCurrent || externallyCurrent();
        return accepted && !lifetime.expired();
    };
    if (!current()) {
        return;
    }

    const QString errorString = result.errorString;
    const DocumentSessionDirectMediaNavigationRefreshApplication application
        = documentSessionDirectMediaNavigationRefreshApplication(
            sourceKind, previousSnapshot, std::move(result));
    if (!application.known) {
        qCDebug(kiriviewNavigationLog) << "direct media navigation refresh failed"
                                       << "error" << errorString;
    } else {
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation refresh finished"
            << "candidates" << application.candidates.size() << "currentNumber"
            << application.boundaryState.currentNumber << "count" << application.boundaryState.count
            << "canPrevious" << application.boundaryState.canOpenPrevious << "canNext"
            << application.boundaryState.canOpenNext;
    }

    invokeIfSet(ports.setDirectMediaNavigation, application.boundaryState, application.known,
        application.candidates);
    if (!current()) {
        return;
    }
    invokeIfSet(ports.applyRevealAction, application.revealAction);
    if (!current()) {
        return;
    }
    invokeIfSet(ports.publishProjection);
    if (!current()) {
        return;
    }
    if (application.schedulePredecode) {
        invokeIfSet(ports.schedulePredecode, QUrl());
    }
}

void DocumentSessionDirectMediaNavigationApplicationRuntime::applyOpen(
    const QUrl& activeDirectMediaCursorUrl, DocumentSessionDirectMediaNavigationOpenResult result,
    const DocumentSessionDirectMediaNavigationApplicationControl& control)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionDirectMediaNavigationApplicationPorts ports = m_ports;
    const std::function<bool()> externallyCurrent = control.isCurrent;
    const std::function<bool()> externallyRouteContinuationCurrent
        = control.routeContinuationIsCurrent ? control.routeContinuationIsCurrent
                                             : externallyCurrent;
    const auto current = [lifetime, externallyCurrent]() {
        if (lifetime.expired()) {
            return false;
        }
        const bool accepted = !externallyCurrent || externallyCurrent();
        return accepted && !lifetime.expired();
    };
    const auto routeContinuationCurrent = [lifetime, externallyRouteContinuationCurrent]() {
        if (lifetime.expired()) {
            return false;
        }
        const bool accepted
            = !externallyRouteContinuationCurrent || externallyRouteContinuationCurrent();
        return accepted && !lifetime.expired();
    };
    if (!current()) {
        return;
    }

    const QString errorString = result.errorString;
    const DocumentSessionDirectMediaNavigationOpenApplication application
        = documentSessionDirectMediaNavigationOpenApplication(
            activeDirectMediaCursorUrl, std::move(result));
    if (!application.known) {
        qCDebug(kiriviewNavigationLog) << "direct media navigation open failed"
                                       << "error" << errorString;
    } else {
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation open finished"
            << "candidates" << application.candidates.size() << "currentNumber"
            << application.boundaryState.currentNumber << "count" << application.boundaryState.count
            << "targetUrl" << application.routeTargetUrl.value_or(QUrl());
    }

    invokeIfSet(ports.setDirectMediaNavigation, application.boundaryState, application.known,
        application.candidates);
    if (!current()) {
        return;
    }
    invokeIfSet(ports.applyRevealAction, application.revealAction);
    if (!current()) {
        return;
    }
    invokeIfSet(ports.publishProjection);
    if (!current()) {
        return;
    }
    if (application.schedulePredecode) {
        invokeIfSet(ports.schedulePredecode, application.routeTargetUrl.value_or(QUrl()));
        if (!current()) {
            return;
        }
    }
    if (application.routeTargetUrl.has_value()) {
        invokeIfSet(ports.routeMediaUrl, *application.routeTargetUrl,
            std::function<bool()>(routeContinuationCurrent));
    }
}
}
