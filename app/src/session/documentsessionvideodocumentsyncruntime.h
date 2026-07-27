// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONVIDEODOCUMENTSYNCRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONVIDEODOCUMENTSYNCRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/directmediacursor.h"
#include "session/documentsessionvideodocumentsync.h"

#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
using DocumentSessionVideoDocumentSyncRuntimeInput = DocumentSessionVideoDocumentSyncInput;

struct DocumentSessionVideoDocumentSyncRuntimePorts
{
    std::function<void()> clearDirectMediaCursor;
    std::function<void(const QUrl&)> setSourceIdentity;
    std::function<bool(DocumentSessionKind)> setDocumentKind;
    std::function<void()> clearDirectMediaNavigation;
    std::function<DirectMediaConfirmation(const QUrl&)> confirmDirectVideoCursor;
    std::function<void()> refreshDirectMediaNavigation;
    std::function<void()> recomputePublicProjection;
};

struct DocumentSessionVideoDocumentSyncRuntimeControl
{
    std::function<bool()> isCurrent;
};

class DocumentSessionVideoDocumentSyncRuntime final
{
public:
    explicit DocumentSessionVideoDocumentSyncRuntime(
        DocumentSessionVideoDocumentSyncRuntimePorts ports = {});
    ~DocumentSessionVideoDocumentSyncRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionVideoDocumentSyncRuntime)

    void sync(
        DocumentSessionKind documentKind, const DocumentSessionPublicVideoLeafSnapshot& video);
    void sync(const DocumentSessionVideoDocumentSyncRuntimeInput& input,
        const DocumentSessionVideoDocumentSyncRuntimeControl& control = {});

private:
    void apply(const DocumentSessionVideoDocumentSyncPlan& plan, quint64 syncRevision,
        const DocumentSessionVideoDocumentSyncRuntimeControl& control);

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionVideoDocumentSyncRuntimePorts m_ports;
    ImageAsyncTicket m_syncAdmission;
};
}

#endif
