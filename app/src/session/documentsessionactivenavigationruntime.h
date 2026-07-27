// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONACTIVENAVIGATIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONACTIVENAVIGATIONRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/activenavigationprojection.h"
#include "session/documentsessiontypes.h"

#include <functional>
#include <memory>

namespace kiriview {
struct DocumentSessionActiveNavigationRuntimePorts
{
    std::function<void(ActiveNavigationRevealContext)> setRevealContext;
    std::function<void()> recomputePublicProjection;
    std::function<void()> openPreviousDirectMedia;
    std::function<void()> openNextDirectMedia;
    std::function<void(int)> openDirectMediaAtNumber;
    std::function<void()> openPreviousImageDocumentPage;
    std::function<void()> openNextImageDocumentPage;
    std::function<void(int)> openImageDocumentPageAtNumber;
};

class DocumentSessionActiveNavigationRuntime final
{
public:
    explicit DocumentSessionActiveNavigationRuntime(
        DocumentSessionActiveNavigationRuntimePorts ports = {});
    ~DocumentSessionActiveNavigationRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionActiveNavigationRuntime)

    ActiveNavigationDispatchOutcome dispatch(ActiveNavigationSourceKind sourceKind,
        ActiveNavigationSnapshot snapshot, ActiveNavigationDispatchRequest request,
        ActiveNavigationRevealContext context);
    void setPendingRevealContext(ActiveNavigationRevealContext context);
    ActiveNavigationRevealContext takePendingRevealContext(
        ActiveNavigationRevealIntent fallbackIntent);
    void setRevealContext(ActiveNavigationRevealContext context);
    void clearRevealContextIfUnavailable(ActiveNavigationSnapshot snapshot);

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionActiveNavigationRuntimePorts m_ports;
    ImageAsyncTicket m_dispatchAdmission;
    ActiveNavigationRevealContext m_pendingRevealContext;
};
}

#endif
