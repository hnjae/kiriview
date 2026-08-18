// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationactionruntime.h"

#include "applicationcommandportsource.h"
#include "applicationshortcutruntime.h"
#include "kiriviewapplicationactions.h"

#include <KirigamiActionCollection>
#include <QIcon>

#include <utility>

namespace {
namespace Actions = kiriview::ApplicationActions;

ImageActionAvailabilityInput imageActionAvailabilityInput(
    const Actions::ApplicationActionStateSnapshot& snapshot)
{
    const kiriview::DocumentSessionActionAvailabilityFacts& facts
        = snapshot.documentSession.availability;
    const Actions::ApplicationActionUiGateSnapshot& gates = snapshot.uiGates;
    const bool currentAuthoritativeImage = facts.imageReady
        && snapshot.documentSession.imagePresentationPhase
            == kiriview::ImagePresentationPhase::CurrentAuthoritative;
    return ImageActionAvailabilityInput {
        currentAuthoritativeImage,
        snapshot.documentSession.fileDeletionInProgress,
        gates.helpDialogOpen,
        gates.textInputFocused,
        currentAuthoritativeImage && snapshot.documentSession.imagePannable,
        facts.containerNavigationAvailable,
        facts.twoPageModeActive,
        facts.twoPageModeAvailable,
        facts.rightToLeftReadingActive,
        facts.rightToLeftReadingAvailable,
    };
}

Actions::ApplicationActionStateInput actionStateInput(
    const Actions::ApplicationActionStateSnapshot& snapshot,
    const ImageActionAvailabilityProjection& projection)
{
    const kiriview::DocumentSessionActionAvailabilityFacts& facts
        = snapshot.documentSession.availability;
    const kiriview::DocumentSessionActionStateSnapshot& document = snapshot.documentSession;
    const Actions::ApplicationActionUiGateSnapshot& gates = snapshot.uiGates;
    const kiriview::ActiveNavigationSnapshot& activeNavigation = document.activeNavigation;
    const bool activeNavigationActionsAvailable = activeNavigation.available
        && activeNavigation.known && activeNavigation.count > 0 && !document.fileDeletionInProgress
        && projection.helpShortcutsEnabled;

    Actions::ApplicationActionStateInput input;
    input.uiGateRevision = snapshot.uiGateRevision;
    input.helpActionsEnabled = projection.helpShortcutsEnabled;
    input.readyActionsEnabled = projection.canUseReadyActions;
    input.zoomInputEditable = document.activeZoom.editable;
    input.transformActionsEnabled = projection.canUseTransformActions;
    input.twoPageModeActionsEnabled = projection.canUseTwoPageModeActions;
    input.rightToLeftReadingActionsEnabled = projection.canUseRightToLeftReadingActions;
    input.containerNavigationActionsEnabled = projection.containerShortcutsEnabled;
    input.displayedMediaOpenWithAvailable = document.displayedMediaOpenWithAvailable;
    input.displayedFileDeletionAvailable = document.displayedFileDeletionAvailable;
    input.fileDeletionInProgress = document.fileDeletionInProgress;
    input.activeNavigationAvailable = activeNavigation.available;
    input.activeNavigationKnown = activeNavigation.known;
    input.activeNavigationHasTargets = activeNavigation.count > 0;
    input.activeNavigationEditable = activeNavigation.editable;
    input.canOpenPreviousActiveNavigation = activeNavigation.canOpenPrevious;
    input.canOpenNextActiveNavigation = activeNavigation.canOpenNext;
    input.atKnownFirstActiveNavigation = activeNavigation.atKnownFirst;
    input.atKnownLastActiveNavigation = activeNavigation.atKnownLast;
    input.fitModeSelected = facts.fitModeSelected;
    input.fitHeightModeSelected = facts.fitHeightModeSelected;
    input.fitWidthModeSelected = facts.fitWidthModeSelected;
    input.twoPageModeActive = projection.twoPageModeActive;
    input.rightToLeftReadingActive = projection.rightToLeftReadingActive;
    input.textInputFocused = gates.textInputFocused;
    input.infoPanelVisible = gates.infoPanelVisible;
    input.thumbnailPanelVisible = gates.thumbnailPanelVisible;
    input.fullscreen = gates.fullscreen;
    input.applicationMenuShortcutEnabled = gates.applicationMenuShortcutEnabled;
    input.showMenubarActionEnabled = gates.showMenubarActionEnabled;
    input.directMediaNavigationBoundaryActive = document.activeNavigationBoundaryScope
        == kiriview::ActiveNavigationBoundaryScope::DirectMedia;
    input.viewerShortcutsEnabled = projection.viewerShortcutsEnabled;
    input.readyShortcutsEnabled = projection.readyShortcutsEnabled;
    input.readyViewerShortcutsEnabled = projection.readyViewerShortcutsEnabled;
    input.twoPageViewerShortcutsEnabled = projection.twoPageViewerShortcutsEnabled;
    input.collectionReadingShortcutsEnabled = projection.collectionReadingShortcutsEnabled;
    input.collectionReadingViewerShortcutsEnabled
        = projection.collectionReadingViewerShortcutsEnabled;
    input.transformShortcutsEnabled = projection.transformShortcutsEnabled;
    input.transformViewerShortcutsEnabled = projection.transformViewerShortcutsEnabled;
    input.pannableShortcutsEnabled = projection.pannableShortcutsEnabled;
    input.pannableViewerShortcutsEnabled = projection.pannableViewerShortcutsEnabled;
    input.containerViewerShortcutsEnabled = projection.containerViewerShortcutsEnabled;
    input.activeNavigationActionsAvailable = activeNavigationActionsAvailable;
    input.videoMode = document.videoMode;
    input.videoFileDeletionInProgress = document.fileDeletionInProgress;
    input.videoSeekable = document.videoSeekable;
    input.videoDuration = document.videoDuration;
    return input;
}

Actions::ApplicationCommandRouterInput routerInputForSnapshot(
    const Actions::ApplicationActionStateSnapshot& snapshot,
    const ImageActionAvailabilityProjection& projection)
{
    const kiriview::DocumentSessionActionStateSnapshot& document = snapshot.documentSession;
    Actions::ApplicationCommandRouterInput input;
    input.imagePannable = document.imagePannable;
    input.rightToLeftReadingActive = projection.rightToLeftReadingActive;
    input.videoMode = document.videoMode;
    input.imageDocumentPageNavigationActive = document.activeNavigationBoundaryScope
        == kiriview::ActiveNavigationBoundaryScope::ImageDocumentPage;
    input.atKnownFirstActiveNavigation = document.activeNavigation.atKnownFirst;
    input.canOpenPreviousActiveNavigation = document.activeNavigation.canOpenPrevious;
    return input;
}

Actions::ImageToolbarActionPresentation toolbarActionPresentation(
    Actions::ActionId actionId, const Actions::ApplicationActionStateInput& input)
{
    const Actions::ApplicationActionState state = Actions::applicationActionState(actionId, input);
    return Actions::ImageToolbarActionPresentation {
        state.actionEnabled,
        state.checked,
        state.actionEnabled,
    };
}

Actions::ActionId presentedFitActionId(kiriview::ImageFitModeSelection selection)
{
    switch (selection) {
    case kiriview::ImageFitModeSelection::Fit:
        return Actions::ActionId::ViewFitAction;
    case kiriview::ImageFitModeSelection::FitHeight:
        return Actions::ActionId::ViewFitHeightAction;
    case kiriview::ImageFitModeSelection::FitWidth:
        return Actions::ActionId::ViewFitWidthAction;
    }

    return Actions::ActionId::ViewFitAction;
}

Actions::ImageToolbarPresentationSnapshot currentImageToolbarPresentation(
    const Actions::ApplicationActionStateSnapshot& snapshot,
    const Actions::ApplicationActionStateInput& input)
{
    const kiriview::DocumentSessionActionStateSnapshot& document = snapshot.documentSession;
    Actions::ImageToolbarPresentationSnapshot presentation;
    presentation.phase = kiriview::ImagePresentationPhase::CurrentAuthoritative;
    presentation.collectionControlsVisible = document.imageCollectionControlsVisible;
    presentation.rightToLeftReading
        = toolbarActionPresentation(Actions::ActionId::ViewToggleRightToLeftReadingAction, input);
    presentation.twoPageMode
        = toolbarActionPresentation(Actions::ActionId::ViewToggleTwoPageModeAction, input);
    presentation.presentedFitActionId = presentedFitActionId(document.imageFitModeSelection);
    presentation.fitMode = toolbarActionPresentation(presentation.presentedFitActionId, input);
    presentation.zoom = Actions::ImageToolbarZoomPresentation {
        input.readyActionsEnabled,
        input.readyActionsEnabled && document.activeZoom.editable,
        document.activeZoom.available,
        document.activeZoom.known,
        document.activeZoom.editable,
        document.activeZoom.percent,
        document.activeZoom.minimumManualPercent,
        document.activeZoom.maximumManualPercent,
    };
    return presentation;
}

void applyCurrentPlacementAndDisableInteractions(
    Actions::ImageToolbarPresentationSnapshot& presentation,
    const Actions::ApplicationActionStateSnapshot& snapshot)
{
    presentation.collectionControlsVisible
        = snapshot.documentSession.imageCollectionControlsVisible;
    presentation.rightToLeftReading.interactionEnabled = false;
    presentation.twoPageMode.interactionEnabled = false;
    presentation.fitMode.interactionEnabled = false;
    presentation.zoom.interactionEnabled = false;
}

void applyCurrentPlacementAndDisableImageInteractions(
    Actions::ImageToolbarPresentationSnapshot& presentation,
    const Actions::ApplicationActionStateSnapshot& snapshot)
{
    presentation.collectionControlsVisible
        = snapshot.documentSession.imageCollectionControlsVisible;
    presentation.fitMode.interactionEnabled = false;
    presentation.zoom.interactionEnabled = false;
}

Actions::ImageToolbarPresentationSnapshot unavailableMediaToolbarPresentation(
    const Actions::ApplicationActionStateSnapshot& snapshot,
    const Actions::ApplicationActionStateInput& input)
{
    const kiriview::DocumentSessionActionStateSnapshot& document = snapshot.documentSession;
    Actions::ImageToolbarPresentationSnapshot presentation;
    presentation.phase = kiriview::ImagePresentationPhase::Unavailable;
    presentation.rightToLeftReading
        = toolbarActionPresentation(Actions::ActionId::ViewToggleRightToLeftReadingAction, input);
    presentation.twoPageMode
        = toolbarActionPresentation(Actions::ActionId::ViewToggleTwoPageModeAction, input);
    presentation.presentedFitActionId = presentedFitActionId(document.imageFitModeSelection);
    presentation.fitMode = toolbarActionPresentation(presentation.presentedFitActionId, input);
    if (document.videoMode) {
        presentation.zoom = Actions::ImageToolbarZoomPresentation {
            document.activeZoom.available,
            false,
            document.activeZoom.available,
            document.activeZoom.known,
            false,
            document.activeZoom.percent,
            document.activeZoom.minimumManualPercent,
            document.activeZoom.maximumManualPercent,
        };
    }
    applyCurrentPlacementAndDisableImageInteractions(presentation, snapshot);
    return presentation;
}
}

namespace kiriview::ApplicationActions {
ApplicationActionRuntime::ApplicationActionRuntime(ApplicationActionHost& host, Callbacks callbacks)
    : m_host(host)
    , m_actionRegistry(host)
    , m_commandRouter()
    , m_menuPresentationRuntime(host, std::move(callbacks.menuPresentationChanged))
    , m_shortcutRuntime(std::make_unique<ApplicationShortcutRuntime>(m_host, m_actionRegistry,
          std::move(callbacks.shortcutRevisionChanged),
          ApplicationShortcutRuntime::TriggerCallbacks {
              std::move(callbacks.unsupportedVideoActionTriggered),
              std::move(callbacks.unsupportedImageActionTriggered),
              [this](bool leftArrow) { return executeHorizontalArrowShortcut(leftArrow); },
              [this](bool leftArrow) { return executeSinglePageArrowShortcut(leftArrow); },
              [this](bool up) { return executeVerticalPanShortcut(up); },
              [this](
                  qint64 deltaMilliseconds) { return executeVideoSeekShortcut(deltaMilliseconds); },
              [this](FixedShortcutDispatchKind kind) { return executeEscapeShortcut(kind); },
          }))
    , m_actionStateChanged(std::move(callbacks.actionStateChanged))
{
}

ApplicationActionRuntime::~ApplicationActionRuntime() = default;

MenuPresentation ApplicationActionRuntime::menuPresentation() const
{
    return m_menuPresentationRuntime.menuPresentation();
}

void ApplicationActionRuntime::setMenuPresentation(MenuPresentation presentation)
{
    m_menuPresentationRuntime.setMenuPresentation(presentation);
}

int ApplicationActionRuntime::shortcutRevision() const
{
    return m_shortcutRuntime->shortcutRevision();
}

QAbstractListModel* ApplicationActionRuntime::shortcutHelpModel() const
{
    return m_shortcutRuntime->shortcutHelpModel();
}

QAbstractListModel* ApplicationActionRuntime::shortcutConfigurationModel() const
{
    return m_shortcutRuntime->shortcutConfigurationModel();
}

QAction* ApplicationActionRuntime::actionForId(ActionId actionId)
{
    return m_actionRegistry.actionForId(actionId);
}

QList<QKeySequence> ApplicationActionRuntime::programWideShortcutsForId(ActionId actionId) const
{
    return m_shortcutRuntime->programWideShortcutsForId(actionId);
}

QList<QKeySequence> ApplicationActionRuntime::viewerLocalShortcutsForId(ActionId actionId) const
{
    return m_shortcutRuntime->viewerLocalShortcutsForId(actionId);
}

bool ApplicationActionRuntime::setProgramWideShortcutsForId(
    ActionId actionId, const QList<QKeySequence>& shortcuts)
{
    return m_shortcutRuntime->setProgramWideShortcutsForId(actionId, shortcuts);
}

bool ApplicationActionRuntime::setViewerLocalShortcutsForId(
    ActionId actionId, const QList<QKeySequence>& shortcuts)
{
    return m_shortcutRuntime->setViewerLocalShortcutsForId(actionId, shortcuts);
}

bool ApplicationActionRuntime::setShortcutTextsForId(
    ActionId actionId, ApplicationShortcutActivationScope scope, const QStringList& portableTexts)
{
    return m_shortcutRuntime->setShortcutTextsForId(actionId, scope, portableTexts);
}

QString ApplicationActionRuntime::menuShortcutTextForId(ActionId actionId) const
{
    return m_shortcutRuntime->menuShortcutTextForId(actionId);
}

int ApplicationActionRuntime::actionStateRevision() const { return m_actionStateRevision; }

bool ApplicationActionRuntime::actionPlacementEnabled(ActionId actionId) const
{
    return applicationActionState(actionId, m_actionStateInput).placementEnabled;
}

QString ApplicationActionRuntime::actionMenuText(ActionId actionId) const
{
    return applicationActionMenuText(actionId, m_actionStateInput);
}

QString ApplicationActionRuntime::actionToolbarText(ActionId actionId) const
{
    return applicationActionToolbarText(actionId);
}

QString ApplicationActionRuntime::actionToolbarTooltipText(ActionId actionId) const
{
    return applicationActionToolbarTooltipText(actionId);
}

const ImageToolbarPresentationSnapshot&
ApplicationActionRuntime::imageToolbarPresentationSnapshot() const
{
    return m_imageToolbarPresentation;
}

ImageToolbarActionPresentation ApplicationActionRuntime::imageToolbarActionPresentation(
    ActionId actionId) const
{
    switch (actionId) {
    case ActionId::ViewToggleRightToLeftReadingAction:
        return m_imageToolbarPresentation.rightToLeftReading;
    case ActionId::ViewToggleTwoPageModeAction:
        return m_imageToolbarPresentation.twoPageMode;
    case ActionId::ViewFitAction:
    case ActionId::ViewFitHeightAction:
    case ActionId::ViewFitWidthAction: {
        ImageToolbarActionPresentation presentation = m_imageToolbarPresentation.fitMode;
        presentation.appearanceChecked = presentation.appearanceChecked
            && actionId == m_imageToolbarPresentation.presentedFitActionId;
        return presentation;
    }
    default:
        return {};
    }
}

void ApplicationActionRuntime::setActionStateSnapshot(
    const ApplicationActionStateSnapshot& snapshot)
{
    m_actionStateSnapshot = snapshot;
    m_imageActionProjection
        = imageActionAvailabilityProjection(imageActionAvailabilityInput(snapshot));
    setActionStateInput(actionStateInput(snapshot, m_imageActionProjection));
}

void ApplicationActionRuntime::setActionStateInput(const ApplicationActionStateInput& input)
{
    m_actionStateInput = input;
    applyActionState();
    m_shortcutRuntime->setActionStateInput(m_actionStateInput);
    updateImageToolbarPresentation();
    ++m_actionStateRevision;
    if (m_actionStateChanged) {
        m_actionStateChanged();
    }
}

void ApplicationActionRuntime::setCommandPortSource(ApplicationCommandPortSource* source)
{
    m_commandPortSource = source;
}

ApplicationCommandRouterInput ApplicationActionRuntime::commandRouterInput() const
{
    return routerInputForSnapshot(m_actionStateSnapshot, m_imageActionProjection);
}

bool ApplicationActionRuntime::rightToLeftReadingActive() const
{
    return m_imageActionProjection.rightToLeftReadingActive;
}

NavigationPresentationProjection ApplicationActionRuntime::navigationPresentationProjection() const
{
    return kiriview::ApplicationActions::navigationPresentationProjection(
        m_imageActionProjection.rightToLeftReadingActive);
}

void ApplicationActionRuntime::handleActionTriggered(ActionId actionId) const
{
    m_commandRouter.handleActionTriggered(actionId, commandRouterInput(), commandRouterPorts());
}

bool ApplicationActionRuntime::executeHorizontalArrowShortcut(bool leftArrow) const
{
    return m_commandRouter.executeHorizontalArrowShortcut(
        commandRouterInput(), commandRouterPorts(), leftArrow);
}

bool ApplicationActionRuntime::executeSinglePageArrowShortcut(bool leftArrow) const
{
    return m_commandRouter.executeSinglePageArrowShortcut(
        commandRouterInput(), commandRouterPorts(), leftArrow);
}

bool ApplicationActionRuntime::executeVerticalPanShortcut(bool up) const
{
    return m_commandRouter.executeVerticalPanShortcut(
        commandRouterInput(), commandRouterPorts(), up);
}

bool ApplicationActionRuntime::executeVideoSeekShortcut(qint64 deltaMilliseconds) const
{
    return m_commandRouter.executeVideoSeekShortcut(
        commandRouterInput(), commandRouterPorts(), deltaMilliseconds);
}

bool ApplicationActionRuntime::executeEscapeShortcut(FixedShortcutDispatchKind kind) const
{
    return m_commandRouter.executeEscapeShortcut(kind, commandRouterPorts());
}

void ApplicationActionRuntime::setShortcutHost(QObject* host)
{
    m_shortcutRuntime->setShortcutHost(host);
}

void ApplicationActionRuntime::setupActions()
{
    m_host.mainActionCollection()->setComponentDisplayName(QStringLiteral("KiriView"));

    const auto addAction = [this](const Actions::ActionDefinition& definition) {
        const QString name = QString::fromLatin1(definition.name);
        const QList<QKeySequence> shortcuts
            = Actions::defaultShortcuts(definition.defaultProgramWideShortcuts);
        QAction* registeredAction = nullptr;

        switch (definition.kind) {
        case Actions::RegistrationKind::Existing:
            if (QAction* action = m_actionRegistry.collectionAction(name)) {
                registeredAction = finishRegisteredAction(action, action->text(), shortcuts);
            }
            break;
        case Actions::RegistrationKind::Inherited:
            registeredAction = m_actionRegistry.collectionAction(name);
            break;
        case Actions::RegistrationKind::Registered:
            registeredAction = addRegisteredAction(name, Actions::localizedString(definition.text),
                Actions::latin1String(definition.iconName), shortcuts);
            break;
        case Actions::RegistrationKind::ShowMenubar:
            registeredAction = addStandardAction(
                definition.actionType, name, Actions::localizedString(definition.text), shortcuts);
            m_menuPresentationRuntime.bindShowMenuBarAction(registeredAction);
            break;
        case Actions::RegistrationKind::Standard:
            registeredAction = addStandardAction(
                definition.actionType, name, Actions::localizedString(definition.text), shortcuts);
            break;
        }

        if (registeredAction != nullptr
            && definition.shortcutConfigurability
                == Actions::ShortcutConfigurability::NonConfigurable) {
            KirigamiActionCollection::setShortcutsConfigurable(registeredAction, false);
        }
        m_actionRegistry.registerAction(definition, registeredAction);
        if (registeredAction != nullptr) {
            QObject::connect(registeredAction, &QAction::triggered, m_host.actionContext(),
                [this, actionId = definition.actionId, registeredAction]() {
                    handleActionTriggered(actionId, registeredAction);
                });
        }
    };

    for (const Actions::ActionDefinition& definition : Actions::definitions()) {
        addAction(definition);
    }

    m_host.readActionSettings();
    m_menuPresentationRuntime.syncFromSettings();
    m_shortcutRuntime->setup();
    applyActionState();
    m_shortcutRuntime->setActionStateInput(m_actionStateInput);
}

QAction* ApplicationActionRuntime::addRegisteredAction(const QString& name, const QString& text,
    const QString& iconName, const QList<QKeySequence>& defaultShortcuts)
{
    auto* action = new QAction(m_host.actionContext());
    action->setObjectName(name);
    action->setText(text);
    if (!iconName.isEmpty()) {
        action->setIcon(QIcon::fromTheme(iconName));
    }

    m_host.mainActionCollection()->addAction(name, action);
    KirigamiActionCollection::setDefaultShortcuts(action, defaultShortcuts);
    QObject::connect(action, &QAction::changed, m_host.actionContext(),
        [this, action]() { handleActionChanged(action); });
    return action;
}

QAction* ApplicationActionRuntime::addStandardAction(KStandardActions::StandardAction actionType,
    const QString& name, const QString& text, const QList<QKeySequence>& defaultShortcuts)
{
    QAction* action = m_host.mainActionCollection()->addAction(
        actionType, name, m_host.actionContext(), [](bool) { });
    return finishRegisteredAction(action, text, defaultShortcuts);
}

QAction* ApplicationActionRuntime::finishRegisteredAction(
    QAction* action, const QString& text, const QList<QKeySequence>& defaultShortcuts)
{
    action->setText(text);
    KirigamiActionCollection::setDefaultShortcuts(action, defaultShortcuts);
    QObject::connect(action, &QAction::changed, m_host.actionContext(),
        [this, action]() { handleActionChanged(action); });
    return action;
}

void ApplicationActionRuntime::handleActionChanged(QAction* changedAction)
{
    if (m_applyingActionState) {
        return;
    }
    m_shortcutRuntime->handleActionChanged(changedAction);
}

void ApplicationActionRuntime::handleActionTriggered(ActionId actionId, QAction* triggeredAction)
{
    if (triggeredAction == nullptr || !triggeredAction->isEnabled()) {
        return;
    }
    handleActionTriggered(actionId);
}

void ApplicationActionRuntime::applyActionState()
{
    m_applyingActionState = true;
    const QList<RegisteredApplicationAction> registeredActions
        = m_actionRegistry.registeredActions();
    for (const RegisteredApplicationAction& registeredAction : registeredActions) {
        QAction* action = registeredAction.action;
        const ApplicationActionState state
            = applicationActionState(registeredAction.actionId, m_actionStateInput);
        action->setEnabled(state.actionEnabled);
        if (registeredAction.actionId == ActionId::OptionsShowMenubarAction) {
            continue;
        }
        action->setCheckable(state.checkable);
        if (state.checkable) {
            action->setChecked(state.checked);
        }
    }
    m_applyingActionState = false;
}

void ApplicationActionRuntime::resetImageToolbarPresentationHistory()
{
    m_imageToolbarPresentation = {};
    m_lastCurrentImageToolbarPresentation.reset();
}

void ApplicationActionRuntime::updateImageToolbarPresentation()
{
    switch (m_actionStateSnapshot.documentSession.imagePresentationPhase) {
    case kiriview::ImagePresentationPhase::CurrentAuthoritative:
        m_imageToolbarPresentation
            = currentImageToolbarPresentation(m_actionStateSnapshot, m_actionStateInput);
        m_lastCurrentImageToolbarPresentation = m_imageToolbarPresentation;
        break;
    case kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative:
        if (m_lastCurrentImageToolbarPresentation.has_value()) {
            m_imageToolbarPresentation = *m_lastCurrentImageToolbarPresentation;
            m_imageToolbarPresentation.phase
                = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
            applyCurrentPlacementAndDisableInteractions(
                m_imageToolbarPresentation, m_actionStateSnapshot);
            break;
        }
        m_imageToolbarPresentation
            = unavailableMediaToolbarPresentation(m_actionStateSnapshot, m_actionStateInput);
        break;
    case kiriview::ImagePresentationPhase::Unavailable:
        m_lastCurrentImageToolbarPresentation.reset();
        m_imageToolbarPresentation
            = unavailableMediaToolbarPresentation(m_actionStateSnapshot, m_actionStateInput);
        break;
    }
}

ApplicationCommandRouterPorts ApplicationActionRuntime::commandRouterPorts() const
{
    if (m_commandPortSource == nullptr) {
        return {};
    }
    return applicationCommandRouterPorts(*m_commandPortSource);
}

}
