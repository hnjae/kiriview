// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONACTIONRUNTIME_H
#define KIRIVIEW_APPLICATIONACTIONRUNTIME_H

#include "applicationactionhost.h"
#include "applicationactionregistry.h"
#include "applicationactionstatepolicy.h"
#include "applicationcommandrouter.h"
#include "applicationmenupresentationruntime.h"
#include "applicationshortcutpolicy.h"
#include "imageactionavailabilitypolicy.h"
#include "navigationpresentationprojection.h"
#include "session/documentsessiontypes.h"

#include <KStandardActions>
#include <QAbstractListModel>
#include <QAction>
#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview::ApplicationActions {
struct ActionDefinition;
class ApplicationCommandPortSource;
class ApplicationShortcutRuntime;

struct ApplicationActionUiGateSnapshot
{
    bool helpDialogOpen = false;
    bool textInputFocused = false;
    bool infoPanelVisible = false;
    bool thumbnailPanelVisible = false;
    bool fullscreen = false;
    bool applicationMenuShortcutEnabled = false;
    bool showMenubarActionEnabled = true;
};

struct ApplicationActionStateSnapshot
{
    quint64 uiGateRevision = 0;
    kiriview::DocumentSessionActionStateSnapshot documentSession;
    ApplicationActionUiGateSnapshot uiGates;
};

class ApplicationActionRuntime final
{
public:
    struct Callbacks
    {
        std::function<void()> menuPresentationChanged;
        std::function<void()> shortcutRevisionChanged;
        std::function<void()> actionStateChanged;
        std::function<void(ActionId)> unsupportedVideoActionTriggered;
        std::function<void(ActionId)> unsupportedImageActionTriggered;
    };

    explicit ApplicationActionRuntime(ApplicationActionHost& host, Callbacks callbacks = {});
    ~ApplicationActionRuntime();
    Q_DISABLE_COPY_MOVE(ApplicationActionRuntime)

    [[nodiscard]] MenuPresentation menuPresentation() const;
    void setMenuPresentation(MenuPresentation presentation);
    [[nodiscard]] int shortcutRevision() const;
    [[nodiscard]] QAbstractListModel* shortcutHelpModel() const;

    QAction* actionForId(ActionId actionId);
    [[nodiscard]] QList<QKeySequence> programWideShortcutsForId(ActionId actionId) const;
    [[nodiscard]] QList<QKeySequence> viewerLocalShortcutsForId(ActionId actionId) const;
    bool setViewerLocalShortcutsForId(ActionId actionId, const QList<QKeySequence>& shortcuts);
    [[nodiscard]] QString menuShortcutTextForId(ActionId actionId) const;
    [[nodiscard]] int actionStateRevision() const;
    [[nodiscard]] bool actionPlacementEnabled(ActionId actionId) const;
    [[nodiscard]] QString actionMenuText(ActionId actionId) const;
    [[nodiscard]] QString actionToolbarText(ActionId actionId) const;
    [[nodiscard]] QString actionToolbarTooltipText(ActionId actionId) const;
    void setActionStateSnapshot(const ApplicationActionStateSnapshot& snapshot);
    void setActionStateInput(const ApplicationActionStateInput& input);
    void setCommandPortSource(ApplicationCommandPortSource* source);
    [[nodiscard]] ApplicationCommandRouterInput commandRouterInput() const;
    [[nodiscard]] bool rightToLeftReadingActive() const;
    [[nodiscard]] NavigationPresentationProjection navigationPresentationProjection() const;
    void handleActionTriggered(ActionId actionId) const;
    [[nodiscard]] bool executeHorizontalArrowShortcut(bool leftArrow) const;
    [[nodiscard]] bool executeSinglePageArrowShortcut(bool leftArrow) const;
    [[nodiscard]] bool executeVerticalPanShortcut(bool up) const;
    [[nodiscard]] bool executeVideoSeekShortcut(qint64 deltaMilliseconds) const;
    void setShortcutHost(QObject* host);

    void setupActions();

private:
    QAction* addRegisteredAction(const QString& name, const QString& text, const QString& iconName,
        const QList<QKeySequence>& defaultShortcuts = {});
    QAction* addStandardAction(KStandardActions::StandardAction actionType, const QString& name,
        const QString& text, const QList<QKeySequence>& defaultShortcuts);
    QAction* finishRegisteredAction(QAction* registeredAction, const QString& text,
        const QList<QKeySequence>& defaultShortcuts);
    void applyActionState();
    [[nodiscard]] ApplicationCommandRouterPorts commandRouterPorts() const;
    void handleActionChanged(QAction* changedAction);
    void handleActionTriggered(ActionId actionId, QAction* triggeredAction);

    ApplicationActionHost& m_host;
    ApplicationActionRegistry m_actionRegistry;
    ApplicationCommandRouter m_commandRouter;
    ApplicationMenuPresentationRuntime m_menuPresentationRuntime;
    std::unique_ptr<ApplicationShortcutRuntime> m_shortcutRuntime;
    ImageActionAvailabilityProjection m_imageActionProjection;
    ApplicationActionStateSnapshot m_actionStateSnapshot;
    ApplicationActionStateInput m_actionStateInput;
    ApplicationCommandPortSource* m_commandPortSource = nullptr;
    int m_actionStateRevision = 0;
    bool m_applyingActionState = false;
    std::function<void()> m_actionStateChanged;
};
}

#endif
