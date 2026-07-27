// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONAPPLICATIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIANAVIGATIONAPPLICATIONRUNTIME_H

#include "session/documentsessiondirectmedianavigationworkflow.h"

#include <QUrl>
#include <functional>
#include <memory>
#include <vector>

namespace kiriview {
struct DocumentSessionDirectMediaNavigationApplicationPorts
{
    std::function<void(
        DirectMediaNavigationBoundaryState, bool, std::vector<DirectMediaNavigationCandidate>)>
        setDirectMediaNavigation;
    std::function<void(DocumentSessionDirectMediaNavigationRevealAction)> applyRevealAction;
    std::function<void()> publishProjection;
    std::function<void(const QUrl&)> schedulePredecode;
    std::function<void(const QUrl&, std::function<bool()>)> routeMediaUrl;
};

struct DocumentSessionDirectMediaNavigationApplicationControl
{
    std::function<bool()> isCurrent;
    std::function<bool()> routeContinuationIsCurrent;
};

class DocumentSessionDirectMediaNavigationApplicationRuntime final
{
public:
    explicit DocumentSessionDirectMediaNavigationApplicationRuntime(
        DocumentSessionDirectMediaNavigationApplicationPorts ports);
    ~DocumentSessionDirectMediaNavigationApplicationRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionDirectMediaNavigationApplicationRuntime)

    void applyInactiveRefresh(
        const DocumentSessionDirectMediaNavigationApplicationControl& control = {});
    void applyRefresh(ActiveNavigationSourceKind sourceKind,
        ActiveNavigationSnapshot previousSnapshot,
        DocumentSessionDirectMediaNavigationRefreshResult result,
        const DocumentSessionDirectMediaNavigationApplicationControl& control = {});
    void applyOpen(const QUrl& activeDirectMediaCursorUrl,
        DocumentSessionDirectMediaNavigationOpenResult result,
        const DocumentSessionDirectMediaNavigationApplicationControl& control = {});

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionDirectMediaNavigationApplicationPorts m_ports;
};
}

#endif
