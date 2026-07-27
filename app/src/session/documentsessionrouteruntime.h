// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONROUTERUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONROUTERUNTIME_H

#include "async/imageasyncoperationstate.h"
#include "async/imageasyncticket.h"
#include "location/imageurl.h"
#include "session/documentsessionrouteplan.h"

#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
struct DocumentSessionRouteSessionPorts
{
    std::function<void()> cancelMediaOpenWith;
    std::function<void()> clearSessionErrorString;
    std::function<void(const std::function<void()>&)> executeWithRoutingSuppressed;
    std::function<void()> routeCompleted;
};

struct DocumentSessionRouteDirectMediaPorts
{
    std::function<void()> cancelDirectMediaNavigation;
    std::function<void()> cancelMediaDeletion;
    std::function<void()> clearDirectMediaNavigation;
    std::function<bool()> clearDirectMediaCursor;
    std::function<bool(const ResolvedNavigationSource&)> setDirectVideoCursor;
    std::function<bool(const ResolvedNavigationSource&)> requestDirectImageCursor;
    std::function<bool()> syncDirectImageCursorFromDocument;
    std::function<bool()> directMediaNavigationActive;
    std::function<void()> refreshDirectMediaNavigation;
};

struct DocumentSessionRouteDocumentPorts
{
    std::function<void()> clearImageDocument;
    std::function<void()> leaveVideoMode;
    std::function<void()> enterEmptyDocument;
    std::function<void(const ResolvedNavigationSource&)> enterImageDocument;
    std::function<void(const ResolvedNavigationSource&)> enterImageDocumentSameScopeNavigation;
    std::function<void(const ResolvedNavigationSource&)> enterVideoDocument;
};

struct DocumentSessionRouteSourceIdentityPorts
{
    std::function<void()> clearSourceIdentity;
    std::function<void(const QUrl&)> useOriginalSourceIdentity;
    std::function<void()> useImageDocumentSourceIdentity;
};

struct DocumentSessionRouteFollowUpPorts
{
    std::function<void()> recomputePublicProjection;
    std::function<void()> syncMediaPredecodeScope;
};

struct DocumentSessionRouteRuntimePorts
{
    DocumentSessionRouteSessionPorts session;
    DocumentSessionRouteDirectMediaPorts directMedia;
    DocumentSessionRouteDocumentPorts documents;
    DocumentSessionRouteSourceIdentityPorts sourceIdentity;
    DocumentSessionRouteFollowUpPorts followUp;
};

struct DocumentSessionRouteExecutionControl
{
    std::function<bool()> isCurrent;
    std::function<void()> beforePublicProjection;
};

using DocumentSessionRouteSourceResolver = std::function<ResolvedNavigationSource(const QUrl&)>;

class DocumentSessionRouteRuntime final
{
public:
    explicit DocumentSessionRouteRuntime(DocumentSessionRouteRuntimePorts ports = {});
    ~DocumentSessionRouteRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionRouteRuntime)

    [[nodiscard]] bool executeWithSourceResolver(const DocumentSessionRoutePlan& plan,
        const DocumentSessionRouteSourceResolver& resolveSource,
        const DocumentSessionRouteExecutionControl& control = {});

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionRouteRuntimePorts m_ports;
    ImageAsyncTicket m_admission;
    ImageAsyncOperationState m_execution;
};
}

#endif
