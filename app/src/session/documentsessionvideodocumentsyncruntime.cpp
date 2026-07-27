// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionvideodocumentsyncruntime.h"

#include <QUrl>
#include <utility>

namespace kiriview {
DocumentSessionVideoDocumentSyncRuntime::DocumentSessionVideoDocumentSyncRuntime(
    DocumentSessionVideoDocumentSyncRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionVideoDocumentSyncRuntime::~DocumentSessionVideoDocumentSyncRuntime()
{
    m_callbackLifetime.reset();
}

void DocumentSessionVideoDocumentSyncRuntime::sync(
    DocumentSessionKind documentKind, const DocumentSessionPublicVideoLeafSnapshot& video)
{
    sync(DocumentSessionVideoDocumentSyncRuntimeInput { documentKind, video, false });
}

void DocumentSessionVideoDocumentSyncRuntime::sync(
    const DocumentSessionVideoDocumentSyncRuntimeInput& input,
    const DocumentSessionVideoDocumentSyncRuntimeControl& control)
{
    const quint64 syncRevision = m_syncAdmission.next();
    apply(documentSessionVideoDocumentSyncPlan(input), syncRevision, control);
}

void DocumentSessionVideoDocumentSyncRuntime::apply(
    const DocumentSessionVideoDocumentSyncPlan& plan, quint64 syncRevision,
    const DocumentSessionVideoDocumentSyncRuntimeControl& control)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionVideoDocumentSyncRuntimePorts ports = m_ports;
    const std::function<bool()> externallyCurrent = control.isCurrent;
    const auto current = [this, lifetime, syncRevision, externallyCurrent]() {
        if (lifetime.expired() || !m_syncAdmission.accepts(syncRevision)) {
            return false;
        }
        const bool accepted = !externallyCurrent || externallyCurrent();
        return accepted && !lifetime.expired() && m_syncAdmission.accepts(syncRevision);
    };
    if (!current()) {
        return;
    }
    switch (plan.operation) {
    case DocumentSessionVideoDocumentSyncOperation::None:
        return;
    case DocumentSessionVideoDocumentSyncOperation::ClearSessionDirectMedia:
        if (ports.setDocumentKind
            && (!ports.setDocumentKind(DocumentSessionKind::Empty) || !current())) {
            return;
        }
        if (ports.clearDirectMediaCursor) {
            ports.clearDirectMediaCursor();
            if (!current()) {
                return;
            }
        }
        if (ports.setSourceIdentity) {
            ports.setSourceIdentity(QUrl());
            if (!current()) {
                return;
            }
        }
        if (ports.clearDirectMediaNavigation) {
            ports.clearDirectMediaNavigation();
            if (!current()) {
                return;
            }
        }
        break;
    case DocumentSessionVideoDocumentSyncOperation::CommitDirectVideoCursor: {
        const DirectMediaConfirmation confirmation = ports.confirmDirectVideoCursor
            ? ports.confirmDirectVideoCursor(plan.url)
            : DirectMediaConfirmation::Bypassed;
        if (!current() || confirmation != DirectMediaConfirmation::Committed) {
            return;
        }
        if (ports.setSourceIdentity) {
            ports.setSourceIdentity(plan.url);
            if (!current()) {
                return;
            }
        }
        break;
    }
    case DocumentSessionVideoDocumentSyncOperation::CommitOpenedCollectionVideoSource:
        if (ports.setSourceIdentity) {
            ports.setSourceIdentity(plan.url);
            if (!current()) {
                return;
            }
        }
        break;
    }

    if (ports.recomputePublicProjection) {
        ports.recomputePublicProjection();
    }
}
}
