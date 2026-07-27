// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONCOORDINATOR_H
#define KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONCOORDINATOR_H

#include "async/imageasyncticket.h"
#include "navigation/directmedianavigationcandidateprovider.h"
#include "session/activenavigationprojection.h"
#include "session/documentsessiondirectmedianavigationapplicationruntime.h"
#include "session/documentsessiondirectmedianavigationruntime.h"

#include <QUrl>
#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct DocumentSessionDirectMediaNavigationCoordinatorPorts
{
    std::function<bool()> navigationActive;
    std::function<std::optional<DirectMediaScope>()> currentScope;
    std::function<bool(const DirectMediaScope&)> cursorMatches;
    std::function<std::function<bool()>()> captureRefreshTransitionCurrent;
    std::function<std::function<bool()>()> captureOpenTransitionCurrent;
    std::function<QUrl()> activeCursorUrl;
    std::function<ActiveNavigationSourceKind()> activeNavigationSourceKind;
    std::function<ActiveNavigationSnapshot()> activeNavigationSnapshot;
    std::function<void(
        DirectMediaNavigationBoundaryState, bool, std::vector<DirectMediaNavigationCandidate>)>
        setDirectMediaNavigation;
    std::function<void(DocumentSessionDirectMediaNavigationRevealAction)> applyRevealAction;
    std::function<void()> recomputePublicProjection;
    std::function<void(const QUrl&)> schedulePredecode;
    std::function<void(const QUrl&, std::function<bool()>)> openMediaUrl;
};

class DocumentSessionDirectMediaNavigationCoordinator final
{
public:
    DocumentSessionDirectMediaNavigationCoordinator(
        DirectMediaNavigationCandidateProvider provider = {},
        DocumentSessionDirectMediaNavigationCoordinatorPorts ports = {});
    ~DocumentSessionDirectMediaNavigationCoordinator();
    Q_DISABLE_COPY_MOVE(DocumentSessionDirectMediaNavigationCoordinator)

    void cancel();
    [[nodiscard]] std::function<bool()> cancelAndCaptureCurrent();
    void refresh(QObject* receiver);
    void openPrevious(QObject* receiver);
    void openNext(QObject* receiver);
    void openAtNumber(QObject* receiver, int mediaNumber);
    void open(QObject* receiver, DirectMediaNavigationOpenRequest request);

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    std::shared_ptr<ImageAsyncTicket> m_applicationAdmission = std::make_shared<ImageAsyncTicket>();
    DocumentSessionDirectMediaNavigationCoordinatorPorts m_ports;
    DocumentSessionDirectMediaNavigationRuntime m_navigationRuntime;
    DocumentSessionDirectMediaNavigationApplicationRuntime m_applicationRuntime;
};
}

#endif
