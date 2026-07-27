// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONIMAGEDOCUMENTSYNCRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONIMAGEDOCUMENTSYNCRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/documentsessiondirectimagecursorsync.h"
#include "session/documentsessionimagedocumentsync.h"

#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
struct DocumentSessionImageDocumentSyncRuntimeInput
{
    bool routingSource = false;
    DocumentSessionKind documentKind = DocumentSessionKind::Empty;
    bool directImageLoadMayUseImageDocumentSourceScope = false;
    bool directMediaNavigationActive = false;
    bool directMediaNavigationKnown = false;
    DirectMediaCursor directMediaCursor;
    ImageDocumentPageActiveNavigationSnapshot previousPageNavigation;
    DocumentSessionPublicImageLeafSnapshot image;
};

struct DocumentSessionImageDocumentSyncRuntimePorts
{
    std::function<DirectMediaConfirmation(const QUrl&)> confirmDirectImageCursor;
    std::function<bool()> restoreDirectImageCursorAfterFailure;
    std::function<void(const QUrl&)> setSourceIdentity;
    std::function<void(bool)> setFileDeletionInProgress;
    std::function<void()> refreshDirectMediaNavigation;
    std::function<void()> cacheDisplayedMediaPredecodeImages;
    std::function<void()> publishImagePageActiveNavigation;
    std::function<void()> recomputePublicProjection;
};

class DocumentSessionImageDocumentSyncRuntime final
{
public:
    explicit DocumentSessionImageDocumentSyncRuntime(
        DocumentSessionImageDocumentSyncRuntimePorts ports = {});
    ~DocumentSessionImageDocumentSyncRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionImageDocumentSyncRuntime)

    void sync(const DocumentSessionImageDocumentSyncRuntimeInput& input);
    bool syncDirectImageCursor(DocumentSessionKind documentKind, const DirectMediaCursor& cursor,
        const DocumentSessionPublicImageLeafSnapshot& image);

private:
    void apply(const DocumentSessionImageDocumentSyncPlan& plan, quint64 syncRevision);

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionImageDocumentSyncRuntimePorts m_ports;
    ImageAsyncTicket m_syncAdmission;
};
}

#endif
