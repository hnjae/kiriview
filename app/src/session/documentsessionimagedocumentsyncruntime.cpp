// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionimagedocumentsyncruntime.h"

#include <utility>

namespace {
template <typename Current>
bool syncDirectImageCursorWithPorts(kiriview::DocumentSessionKind documentKind,
    const kiriview::DirectMediaCursor& cursor,
    const kiriview::DocumentSessionPublicImageLeafSnapshot& image,
    const kiriview::DocumentSessionImageDocumentSyncRuntimePorts& ports, const Current& current)
{
    const kiriview::DocumentSessionDirectImageCursorSyncPlan plan
        = kiriview::documentSessionDirectImageCursorSyncPlan(
            kiriview::DocumentSessionDirectImageCursorSyncInput { documentKind, cursor, image });
    switch (plan.operation) {
    case kiriview::DocumentSessionDirectImageCursorSyncOperation::None:
        return false;
    case kiriview::DocumentSessionDirectImageCursorSyncOperation::ConfirmDirectImageCursor:
        if (ports.confirmDirectImageCursor) {
            ports.confirmDirectImageCursor(plan.url);
        }
        return false;
    case kiriview::DocumentSessionDirectImageCursorSyncOperation::
        RestoreDirectImageCursorAfterFailure: {
        const bool changed = ports.restoreDirectImageCursorAfterFailure
            ? ports.restoreDirectImageCursorAfterFailure()
            : false;
        return current() && changed;
    }
    }

    return false;
}
}

namespace kiriview {
DocumentSessionImageDocumentSyncRuntime::DocumentSessionImageDocumentSyncRuntime(
    DocumentSessionImageDocumentSyncRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionImageDocumentSyncRuntime::~DocumentSessionImageDocumentSyncRuntime()
{
    m_callbackLifetime.reset();
}

void DocumentSessionImageDocumentSyncRuntime::sync(
    const DocumentSessionImageDocumentSyncRuntimeInput& input)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 syncRevision = m_syncAdmission.next();
    const DocumentSessionImageDocumentSyncRuntimePorts ports = m_ports;
    const auto current = [this, lifetime, syncRevision]() {
        return !lifetime.expired() && m_syncAdmission.accepts(syncRevision);
    };
    if (input.routingSource || input.documentKind != DocumentSessionKind::Image) {
        return;
    }

    const bool directMediaScopeChanged = syncDirectImageCursorWithPorts(
        input.documentKind, input.directMediaCursor, input.image, ports, current);
    if (!current()) {
        return;
    }
    apply(documentSessionImageDocumentSyncPlan(DocumentSessionImageDocumentSyncInput {
              input.routingSource,
              input.documentKind,
              input.directImageLoadMayUseImageDocumentSourceScope,
              input.directMediaNavigationActive,
              input.directMediaNavigationKnown,
              directMediaScopeChanged,
              input.previousPageNavigation,
              input.image,
          }),
        syncRevision);
}

bool DocumentSessionImageDocumentSyncRuntime::syncDirectImageCursor(
    DocumentSessionKind documentKind, const DirectMediaCursor& cursor,
    const DocumentSessionPublicImageLeafSnapshot& image)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 syncRevision = m_syncAdmission.next();
    const DocumentSessionImageDocumentSyncRuntimePorts ports = m_ports;
    const auto current = [this, lifetime, syncRevision]() {
        return !lifetime.expired() && m_syncAdmission.accepts(syncRevision);
    };
    return syncDirectImageCursorWithPorts(documentKind, cursor, image, ports, current);
}

void DocumentSessionImageDocumentSyncRuntime::apply(
    const DocumentSessionImageDocumentSyncPlan& plan, quint64 syncRevision)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionImageDocumentSyncRuntimePorts ports = m_ports;
    const auto current = [this, lifetime, syncRevision]() {
        return !lifetime.expired() && m_syncAdmission.accepts(syncRevision);
    };
    if (!plan.active) {
        return;
    }

    if (plan.setSourceIdentity && ports.setSourceIdentity) {
        ports.setSourceIdentity(plan.sourceIdentityUrl);
        if (!current()) {
            return;
        }
    }
    if (plan.syncFileDeletionProgress && ports.setFileDeletionInProgress) {
        ports.setFileDeletionInProgress(plan.fileDeletionInProgress);
        if (!current()) {
            return;
        }
    }

    switch (plan.directMediaOperation) {
    case DocumentSessionImageDocumentSyncDirectMediaOperation::None:
        break;
    case DocumentSessionImageDocumentSyncDirectMediaOperation::RefreshDirectMediaNavigation:
        if (ports.refreshDirectMediaNavigation) {
            ports.refreshDirectMediaNavigation();
            if (!current()) {
                return;
            }
        }
        break;
    case DocumentSessionImageDocumentSyncDirectMediaOperation::CacheDisplayedMediaPredecodeImages:
        if (ports.cacheDisplayedMediaPredecodeImages) {
            ports.cacheDisplayedMediaPredecodeImages();
            if (!current()) {
                return;
            }
        }
        break;
    }

    switch (plan.projectionOperation) {
    case DocumentSessionImageDocumentSyncProjectionOperation::None:
        return;
    case DocumentSessionImageDocumentSyncProjectionOperation::RecomputePublicProjection:
        if (ports.recomputePublicProjection) {
            ports.recomputePublicProjection();
        }
        return;
    case DocumentSessionImageDocumentSyncProjectionOperation::PublishImagePageActiveNavigation:
        if (ports.publishImagePageActiveNavigation) {
            ports.publishImagePageActiveNavigation();
        }
        return;
    }
}
}
