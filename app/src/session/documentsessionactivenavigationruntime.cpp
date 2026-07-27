// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionactivenavigationruntime.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace {
void executeDispatchPlan(kiriview::ActiveNavigationDispatchPlan plan,
    const kiriview::DocumentSessionActiveNavigationRuntimePorts& ports)
{
    if (!plan.shouldDispatch()) {
        return;
    }

    std::visit(
        [&ports](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<Operation,
                              kiriview::OpenPreviousDirectMediaNavigationOperation>) {
                if (ports.openPreviousDirectMedia) {
                    ports.openPreviousDirectMedia();
                }
            } else if constexpr (std::is_same_v<Operation,
                                     kiriview::OpenNextDirectMediaNavigationOperation>) {
                if (ports.openNextDirectMedia) {
                    ports.openNextDirectMedia();
                }
            } else if constexpr (std::is_same_v<Operation,
                                     kiriview::OpenDirectMediaNavigationAtNumberOperation>) {
                if (ports.openDirectMediaAtNumber) {
                    ports.openDirectMediaAtNumber(operation.number);
                }
            } else if constexpr (std::is_same_v<Operation,
                                     kiriview::OpenPreviousImageDocumentPageOperation>) {
                if (ports.openPreviousImageDocumentPage) {
                    ports.openPreviousImageDocumentPage();
                }
            } else if constexpr (std::is_same_v<Operation,
                                     kiriview::OpenNextImageDocumentPageOperation>) {
                if (ports.openNextImageDocumentPage) {
                    ports.openNextImageDocumentPage();
                }
            } else if constexpr (std::is_same_v<Operation,
                                     kiriview::OpenImageDocumentPageAtNumberOperation>) {
                if (ports.openImageDocumentPageAtNumber) {
                    ports.openImageDocumentPageAtNumber(operation.number);
                }
            }
        },
        plan.operation);
}
}

namespace kiriview {
DocumentSessionActiveNavigationRuntime::DocumentSessionActiveNavigationRuntime(
    DocumentSessionActiveNavigationRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionActiveNavigationRuntime::~DocumentSessionActiveNavigationRuntime()
{
    m_callbackLifetime.reset();
}

ActiveNavigationDispatchOutcome DocumentSessionActiveNavigationRuntime::dispatch(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot snapshot,
    ActiveNavigationDispatchRequest request, ActiveNavigationRevealContext context)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 dispatchRevision = m_dispatchAdmission.next();
    const DocumentSessionActiveNavigationRuntimePorts ports = m_ports;
    const auto current = [this, lifetime, dispatchRevision]() {
        return !lifetime.expired() && m_dispatchAdmission.accepts(dispatchRevision);
    };
    const ActiveNavigationDispatchPlan plan
        = activeNavigationDispatchPlan(sourceKind, snapshot, request);
    if (plan.shouldDispatch()) {
        m_pendingRevealContext = context;
        if (ports.setRevealContext) {
            ports.setRevealContext(context);
        }
        if (!current()) {
            return plan.outcome;
        }
    } else {
        m_pendingRevealContext = {};
        if (ports.setRevealContext) {
            ports.setRevealContext({});
        }
        if (!current()) {
            return plan.outcome;
        }
        if (ports.recomputePublicProjection) {
            ports.recomputePublicProjection();
        }
        return plan.outcome;
    }
    executeDispatchPlan(plan, ports);
    return plan.outcome;
}

void DocumentSessionActiveNavigationRuntime::setPendingRevealContext(
    ActiveNavigationRevealContext context)
{
    m_pendingRevealContext = context;
    setRevealContext(context);
}

ActiveNavigationRevealContext DocumentSessionActiveNavigationRuntime::takePendingRevealContext(
    ActiveNavigationRevealIntent fallbackIntent)
{
    const ActiveNavigationRevealContext context
        = m_pendingRevealContext.intent == ActiveNavigationRevealIntent::None
        ? ActiveNavigationRevealContext { fallbackIntent }
        : m_pendingRevealContext;
    m_pendingRevealContext = {};
    return context;
}

void DocumentSessionActiveNavigationRuntime::setRevealContext(ActiveNavigationRevealContext context)
{
    if (m_ports.setRevealContext) {
        m_ports.setRevealContext(context);
    }
}

void DocumentSessionActiveNavigationRuntime::clearRevealContextIfUnavailable(
    ActiveNavigationSnapshot snapshot)
{
    if (!snapshot.available || !snapshot.known) {
        setRevealContext({});
    }
}

}
