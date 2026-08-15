// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiriviewapplication.h"

#include "application/applicationactionhost.h"
#include "application/applicationactionruntime.h"
#include "application/applicationactionsourceattachment.h"
#include "application/applicationcommandportsource.h"
#include "application/applicationcommandrouter.h"
#include "facade/activenavigationboundaryfacts.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "facade/kirivideodocument.h"
#include "facade/kiriwindowshell.h"

#include <KLocalizedString>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace Actions = kiriview::ApplicationActions;

namespace kiriview::ApplicationActions {
class KiriViewApplicationActionHost final : public ApplicationActionHost
{
public:
    explicit KiriViewApplicationActionHost(KiriViewApplication& application)
        : m_application(application)
    {
    }

    ~KiriViewApplicationActionHost() override = default;

    QObject* actionContext() override { return &m_application; }
    KirigamiActionCollection* mainActionCollection() override
    {
        return m_application.applicationMainActionCollection();
    }
    QAction* inheritedAction(const QString& actionName) override
    {
        return m_application.inheritedApplicationAction(actionName);
    }
    void readActionSettings() override { m_application.readApplicationActionSettings(); }

private:
    KiriViewApplication& m_application;
    Q_DISABLE_COPY_MOVE(KiriViewApplicationActionHost)
};

class KiriViewApplicationCommandPortSource final : public ApplicationCommandPortSource
{
public:
    explicit KiriViewApplicationCommandPortSource(KiriViewApplication& application)
        : m_application(application)
    {
    }

    ~KiriViewApplicationCommandPortSource() override = default;

    ApplicationCommandRouterShellPorts commandRouterShellPorts() override;
    ApplicationCommandRouterSessionPorts commandRouterSessionPorts() override;
    ApplicationCommandRouterImageDocumentPorts commandRouterImageDocumentPorts() override;
    ApplicationCommandRouterImagePresentationPorts commandRouterImagePresentationPorts() override;
    ApplicationCommandRouterToolbarPorts commandRouterToolbarPorts() override;
    ApplicationCommandRouterPanelPorts commandRouterPanelPorts() override;
    ApplicationCommandRouterWindowPorts commandRouterWindowPorts() override;
    ApplicationCommandRouterHelpPorts commandRouterHelpPorts() override;
    ApplicationCommandRouterVideoPorts commandRouterVideoPorts() override;

private:
    [[nodiscard]] KiriDocumentSession* documentSession() const;
    [[nodiscard]] KiriImageDocument* imageDocument() const;
    [[nodiscard]] KiriVideoDocument* videoDocument() const;
    void deleteDisplayedFileByMode(KiriDocumentSession::DeletionMode mode);
    void requestImageFitMode(KiriImageDocument::ZoomMode mode);
    void requestPreviousActiveNavigationWithBoundary();
    void requestNextActiveNavigationWithBoundary();
    void emitCurrentBoundaryText(const QString& message, kiriview::NavigationBoundaryEdge edge);
    void emitBoundaryText(const QString& message,
        const kiriview::NavigationBoundaryCorrelation& correlation,
        KiriDocumentSession* originSession);

    KiriViewApplication& m_application;
    Q_DISABLE_COPY_MOVE(KiriViewApplicationCommandPortSource)
};
}

static_assert(static_cast<int>(Actions::MenuPresentation::HamburgerMenu)
    == static_cast<int>(KiriViewApplication::HamburgerMenu));
static_assert(static_cast<int>(Actions::MenuPresentation::MenuBar)
    == static_cast<int>(KiriViewApplication::MenuBar));
static_assert(static_cast<int>(Actions::ApplicationShortcutActivationScope::ProgramWide)
    == static_cast<int>(KiriViewApplication::ProgramWideShortcutScope));
static_assert(static_cast<int>(Actions::ApplicationShortcutActivationScope::ViewerLocal)
    == static_cast<int>(KiriViewApplication::ViewerLocalShortcutScope));

namespace {
struct ActionIdMapping
{
    KiriViewApplication::ActionId facade;
    Actions::ActionId domain;
};

constexpr std::array actionIdMappings {
    ActionIdMapping { KiriViewApplication::FileOpenAction, Actions::ActionId::FileOpenAction },
    ActionIdMapping {
        KiriViewApplication::FileOpenWithAction, Actions::ActionId::FileOpenWithAction },
    ActionIdMapping {
        KiriViewApplication::FileMoveToTrashAction, Actions::ActionId::FileMoveToTrashAction },
    ActionIdMapping { KiriViewApplication::FileDeleteAction, Actions::ActionId::FileDeleteAction },
    ActionIdMapping {
        KiriViewApplication::GoPreviousArchiveAction, Actions::ActionId::GoPreviousArchiveAction },
    ActionIdMapping {
        KiriViewApplication::GoNextArchiveAction, Actions::ActionId::GoNextArchiveAction },
    ActionIdMapping {
        KiriViewApplication::GoPreviousImageAction, Actions::ActionId::GoPreviousImageAction },
    ActionIdMapping {
        KiriViewApplication::GoNextImageAction, Actions::ActionId::GoNextImageAction },
    ActionIdMapping {
        KiriViewApplication::GoFirstImageAction, Actions::ActionId::GoFirstImageAction },
    ActionIdMapping {
        KiriViewApplication::GoLastImageAction, Actions::ActionId::GoLastImageAction },
    ActionIdMapping { KiriViewApplication::ViewZoomInAction, Actions::ActionId::ViewZoomInAction },
    ActionIdMapping {
        KiriViewApplication::ViewZoomOutAction, Actions::ActionId::ViewZoomOutAction },
    ActionIdMapping {
        KiriViewApplication::ViewZoom50PercentAction, Actions::ActionId::ViewZoom50PercentAction },
    ActionIdMapping { KiriViewApplication::ViewZoom100PercentAction,
        Actions::ActionId::ViewZoom100PercentAction },
    ActionIdMapping { KiriViewApplication::ViewZoom200PercentAction,
        Actions::ActionId::ViewZoom200PercentAction },
    ActionIdMapping { KiriViewApplication::ViewFitAction, Actions::ActionId::ViewFitAction },
    ActionIdMapping {
        KiriViewApplication::ViewFitHeightAction, Actions::ActionId::ViewFitHeightAction },
    ActionIdMapping {
        KiriViewApplication::ViewFitWidthAction, Actions::ActionId::ViewFitWidthAction },
    ActionIdMapping { KiriViewApplication::ViewRotateClockwiseAction,
        Actions::ActionId::ViewRotateClockwiseAction },
    ActionIdMapping { KiriViewApplication::ViewRotateCounterclockwiseAction,
        Actions::ActionId::ViewRotateCounterclockwiseAction },
    ActionIdMapping { KiriViewApplication::ViewFlipHorizontallyAction,
        Actions::ActionId::ViewFlipHorizontallyAction },
    ActionIdMapping { KiriViewApplication::ViewFlipVerticallyAction,
        Actions::ActionId::ViewFlipVerticallyAction },
    ActionIdMapping { KiriViewApplication::ViewToggleTwoPageModeAction,
        Actions::ActionId::ViewToggleTwoPageModeAction },
    ActionIdMapping { KiriViewApplication::ViewToggleRightToLeftReadingAction,
        Actions::ActionId::ViewToggleRightToLeftReadingAction },
    ActionIdMapping { KiriViewApplication::ViewToggleInfoPanelAction,
        Actions::ActionId::ViewToggleInfoPanelAction },
    ActionIdMapping { KiriViewApplication::ViewToggleThumbnailPanelAction,
        Actions::ActionId::ViewToggleThumbnailPanelAction },
    ActionIdMapping { KiriViewApplication::ViewGoToContentStartAction,
        Actions::ActionId::ViewGoToContentStartAction },
    ActionIdMapping { KiriViewApplication::ViewGoToContentEndAction,
        Actions::ActionId::ViewGoToContentEndAction },
    ActionIdMapping {
        KiriViewApplication::ViewScanForwardAction, Actions::ActionId::ViewScanForwardAction },
    ActionIdMapping {
        KiriViewApplication::ViewScanBackwardAction, Actions::ActionId::ViewScanBackwardAction },
    ActionIdMapping { KiriViewApplication::ViewToggleVideoPlaybackAction,
        Actions::ActionId::ViewToggleVideoPlaybackAction },
    ActionIdMapping {
        KiriViewApplication::WindowFullscreenAction, Actions::ActionId::WindowFullscreenAction },
    ActionIdMapping {
        KiriViewApplication::HelpShortcutsAction, Actions::ActionId::HelpShortcutsAction },
    ActionIdMapping { KiriViewApplication::OptionsConfigureKeybindingAction,
        Actions::ActionId::OptionsConfigureKeybindingAction },
    ActionIdMapping { KiriViewApplication::OptionsShowMenubarAction,
        Actions::ActionId::OptionsShowMenubarAction },
    ActionIdMapping { KiriViewApplication::OpenApplicationMenuAction,
        Actions::ActionId::OpenApplicationMenuAction },
    ActionIdMapping { KiriViewApplication::FileQuitAction, Actions::ActionId::FileQuitAction },
};

constexpr bool actionIdMappingsAreCompleteAndUnique()
{
    if (actionIdMappings.size() != Actions::actionDefinitionCount
        || actionIdMappings.size() != static_cast<std::size_t>(KiriViewApplication::ActionCount)) {
        return false;
    }

    for (std::size_t index = 0; index < actionIdMappings.size(); ++index) {
        for (std::size_t other = index + 1; other < actionIdMappings.size(); ++other) {
            if (actionIdMappings[index].facade == actionIdMappings[other].facade
                || actionIdMappings[index].domain == actionIdMappings[other].domain) {
                return false;
            }
        }
    }

    return true;
}

static_assert(actionIdMappingsAreCompleteAndUnique());

Actions::NavigationPresentationSlot projectedNavigationPresentationSlot(
    const Actions::NavigationPresentationProjection& projection,
    KiriViewApplication::NavigationPresentationSlot slot)
{
    switch (slot) {
    case KiriViewApplication::LeadingImageActionSlot:
        return projection.leadingImageAction;
    case KiriViewApplication::TrailingImageActionSlot:
        return projection.trailingImageAction;
    case KiriViewApplication::LeadingImageMenuActionSlot:
        return projection.leadingImageMenuAction;
    case KiriViewApplication::TrailingImageMenuActionSlot:
        return projection.trailingImageMenuAction;
    case KiriViewApplication::FirstImageMenuActionSlot:
        return projection.firstImageMenuAction;
    case KiriViewApplication::LastImageMenuActionSlot:
        return projection.lastImageMenuAction;
    case KiriViewApplication::LeadingArchiveMenuActionSlot:
        return projection.leadingArchiveMenuAction;
    case KiriViewApplication::TrailingArchiveMenuActionSlot:
        return projection.trailingArchiveMenuAction;
    case KiriViewApplication::NavigationPresentationSlotCount:
        break;
    }

    return {};
}
}

KiriViewApplication::KiriViewApplication(QObject* parent)
    : AbstractKirigamiApplication(parent)
    , m_actionHost(std::make_unique<Actions::KiriViewApplicationActionHost>(*this))
    , m_commandPortSource(std::make_unique<Actions::KiriViewApplicationCommandPortSource>(*this))
    , m_actionRuntime(std::make_unique<Actions::ApplicationActionRuntime>(*m_actionHost,
          Actions::ApplicationActionRuntime::Callbacks {
              [this]() { Q_EMIT menuPresentationChanged(); },
              [this]() { Q_EMIT shortcutRevisionChanged(); },
              [this]() { Q_EMIT actionStateRevisionChanged(); },
              [this](Actions::ActionId actionId) {
                  Q_EMIT unsupportedVideoActionTriggered(facadeActionId(actionId));
              },
              [this](Actions::ActionId actionId) {
                  Q_EMIT unsupportedImageActionTriggered(facadeActionId(actionId));
              },
          }))
    , m_actionSourceAttachment(
          std::make_unique<Actions::ApplicationActionSourceAttachment>(*m_actionRuntime, *this))
{
    m_actionRuntime->setCommandPortSource(m_commandPortSource.get());
    KiriViewApplication::setupActions();
    m_actionSourceAttachment->setDocumentSessionSnapshotPort({});
}

KiriViewApplication::~KiriViewApplication() = default;

KiriViewApplication::MenuPresentation KiriViewApplication::menuPresentation() const
{
    return facadeMenuPresentation(m_actionRuntime->menuPresentation());
}

void KiriViewApplication::setMenuPresentation(MenuPresentation presentation)
{
    m_actionRuntime->setMenuPresentation(domainMenuPresentation(presentation));
}

int KiriViewApplication::shortcutRevision() const { return m_actionRuntime->shortcutRevision(); }

int KiriViewApplication::actionStateRevision() const
{
    return m_actionRuntime->actionStateRevision();
}

QAbstractListModel* KiriViewApplication::shortcutHelpModel() const
{
    return m_actionRuntime->shortcutHelpModel();
}

QAbstractListModel* KiriViewApplication::shortcutConfigurationModel() const
{
    return m_actionRuntime->shortcutConfigurationModel();
}

Actions::MenuPresentation KiriViewApplication::domainMenuPresentation(MenuPresentation presentation)
{
    if (presentation == MenuBar) {
        return Actions::MenuPresentation::MenuBar;
    }

    return Actions::MenuPresentation::HamburgerMenu;
}

KiriViewApplication::MenuPresentation KiriViewApplication::facadeMenuPresentation(
    Actions::MenuPresentation presentation)
{
    if (presentation == Actions::MenuPresentation::MenuBar) {
        return MenuBar;
    }

    return HamburgerMenu;
}

Actions::ActionId KiriViewApplication::domainActionId(ActionId actionId)
{
    for (const ActionIdMapping& mapping : actionIdMappings) {
        if (mapping.facade == actionId) {
            return mapping.domain;
        }
    }

    return Actions::ActionId::ActionCount;
}

KiriViewApplication::ActionId KiriViewApplication::facadeActionId(Actions::ActionId actionId)
{
    for (const ActionIdMapping& mapping : actionIdMappings) {
        if (mapping.domain == actionId) {
            return mapping.facade;
        }
    }

    return ActionCount;
}

QAction* KiriViewApplication::actionForId(ActionId actionId)
{
    return m_actionRuntime->actionForId(domainActionId(actionId));
}

QList<QKeySequence> KiriViewApplication::programWideShortcutsForId(ActionId actionId) const
{
    return m_actionRuntime->programWideShortcutsForId(domainActionId(actionId));
}

QList<QKeySequence> KiriViewApplication::viewerLocalShortcutsForId(ActionId actionId) const
{
    return m_actionRuntime->viewerLocalShortcutsForId(domainActionId(actionId));
}

bool KiriViewApplication::setProgramWideShortcutsForId(
    ActionId actionId, const QList<QKeySequence>& shortcuts)
{
    return m_actionRuntime->setProgramWideShortcutsForId(domainActionId(actionId), shortcuts);
}

bool KiriViewApplication::setViewerLocalShortcutsForId(
    ActionId actionId, const QList<QKeySequence>& shortcuts)
{
    return m_actionRuntime->setViewerLocalShortcutsForId(domainActionId(actionId), shortcuts);
}

bool KiriViewApplication::setShortcutTextsForId(
    ActionId actionId, ShortcutScope scope, const QStringList& portableTexts)
{
    return m_actionRuntime->setShortcutTextsForId(domainActionId(actionId),
        static_cast<Actions::ApplicationShortcutActivationScope>(scope), portableTexts);
}

QString KiriViewApplication::menuShortcutTextForId(ActionId actionId) const
{
    return m_actionRuntime->menuShortcutTextForId(domainActionId(actionId));
}

bool KiriViewApplication::actionPlacementEnabled(ActionId actionId) const
{
    return m_actionRuntime->actionPlacementEnabled(domainActionId(actionId));
}

QString KiriViewApplication::actionMenuTextForId(ActionId actionId) const
{
    return m_actionRuntime->actionMenuText(domainActionId(actionId));
}

QString KiriViewApplication::actionToolbarTextForId(ActionId actionId) const
{
    return m_actionRuntime->actionToolbarText(domainActionId(actionId));
}

QString KiriViewApplication::actionToolbarTooltipTextForId(ActionId actionId) const
{
    return m_actionRuntime->actionToolbarTooltipText(domainActionId(actionId));
}

KiriViewApplication::ImageToolbarPresentationPhase
KiriViewApplication::imageToolbarPresentationPhase() const
{
    switch (m_actionRuntime->imageToolbarPresentationSnapshot().phase) {
    case kiriview::ImagePresentationPhase::CurrentAuthoritative:
        return ImageToolbarPresentationCurrent;
    case kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative:
        return ImageToolbarPresentationRetainedPrevious;
    case kiriview::ImagePresentationPhase::Unavailable:
        return ImageToolbarPresentationUnavailable;
    }

    return ImageToolbarPresentationUnavailable;
}

bool KiriViewApplication::imageToolbarCollectionControlsVisible() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().collectionControlsVisible;
}

bool KiriViewApplication::imageToolbarActionAppearanceEnabled(ActionId actionId) const
{
    return m_actionRuntime->imageToolbarActionPresentation(domainActionId(actionId))
        .appearanceEnabled;
}

bool KiriViewApplication::imageToolbarActionAppearanceChecked(ActionId actionId) const
{
    return m_actionRuntime->imageToolbarActionPresentation(domainActionId(actionId))
        .appearanceChecked;
}

bool KiriViewApplication::imageToolbarActionInteractionEnabled(ActionId actionId) const
{
    return m_actionRuntime->imageToolbarActionPresentation(domainActionId(actionId))
        .interactionEnabled;
}

KiriViewApplication::ActionId KiriViewApplication::imageToolbarPresentedFitActionId() const
{
    return facadeActionId(m_actionRuntime->imageToolbarPresentationSnapshot().presentedFitActionId);
}

bool KiriViewApplication::imageToolbarZoomAppearanceEnabled() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.appearanceEnabled;
}

bool KiriViewApplication::imageToolbarZoomInteractionEnabled() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.interactionEnabled;
}

bool KiriViewApplication::imageToolbarZoomPercentAvailable() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.available;
}

bool KiriViewApplication::imageToolbarZoomPercentKnown() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.known;
}

bool KiriViewApplication::imageToolbarZoomPercentEditable() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.editable;
}

qreal KiriViewApplication::imageToolbarZoomPercent() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.percent;
}

int KiriViewApplication::imageToolbarZoomMinimumManualPercent() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.minimumManualPercent;
}

int KiriViewApplication::imageToolbarZoomMaximumManualPercent() const
{
    return m_actionRuntime->imageToolbarPresentationSnapshot().zoom.maximumManualPercent;
}

KiriViewApplication::ActionId KiriViewApplication::navigationPresentationActionId(
    NavigationPresentationSlot slot) const
{
    return facadeActionId(projectedNavigationPresentationSlot(
        m_actionRuntime->navigationPresentationProjection(), slot)
            .actionId);
}

KiriViewApplication::ActionId KiriViewApplication::navigationPresentationIconActionId(
    NavigationPresentationSlot slot) const
{
    return facadeActionId(projectedNavigationPresentationSlot(
        m_actionRuntime->navigationPresentationProjection(), slot)
            .iconActionId);
}

QVariantList KiriViewApplication::navigationApplicationMenuActionIds() const
{
    const Actions::NavigationPresentationProjection projection
        = m_actionRuntime->navigationPresentationProjection();

    QVariantList actionIds;
    actionIds.reserve(static_cast<int>(projection.applicationMenuArchiveActionIds.size()));
    for (Actions::ActionId actionId : projection.applicationMenuArchiveActionIds) {
        actionIds.push_back(static_cast<int>(facadeActionId(actionId)));
    }
    return actionIds;
}

void KiriViewApplication::setDocumentSession(QObject* session)
{
    auto* documentSession = qobject_cast<KiriDocumentSession*>(session);
    if (m_documentSession == documentSession) {
        return;
    }

    m_documentSession = documentSession;
    m_actionSourceAttachment->setDocumentSessionSnapshotPort(documentSession == nullptr
            ? kiriview::DocumentSessionActionStateSnapshotPort {}
            : documentSession->actionStateSnapshotPort());
}

void KiriViewApplication::setWindowShell(QObject* shell)
{
    m_windowShell = qobject_cast<KiriWindowShell*>(shell);
}

void KiriViewApplication::updateActionUiGateSnapshot(bool helpDialogOpen, bool textInputFocused,
    bool infoPanelVisible, bool thumbnailPanelVisible, bool fullscreen,
    bool applicationMenuShortcutEnabled, bool showMenubarActionEnabled)
{
    m_actionSourceAttachment->setUiGateSnapshot(Actions::ApplicationActionUiGateSnapshot {
        helpDialogOpen,
        textInputFocused,
        infoPanelVisible,
        thumbnailPanelVisible,
        fullscreen,
        applicationMenuShortcutEnabled,
        showMenubarActionEnabled,
    });
}

void KiriViewApplication::setShortcutHost(QObject* host) { m_actionRuntime->setShortcutHost(host); }

void KiriViewApplication::setupActions()
{
    AbstractKirigamiApplication::setupActions();
    if (m_actionRuntime != nullptr) {
        m_actionRuntime->setupActions();
    }
}

KirigamiActionCollection* KiriViewApplication::applicationMainActionCollection()
{
    return mainCollection();
}

QAction* KiriViewApplication::inheritedApplicationAction(const QString& actionName)
{
    return AbstractKirigamiApplication::action(actionName);
}

void KiriViewApplication::readApplicationActionSettings() { readSettings(); }

Actions::ApplicationCommandRouterShellPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterShellPorts()
{
    Actions::ApplicationCommandRouterShellPorts ports;
    ports.requestOpenDialog = [this]() { Q_EMIT m_application.openDialogRequested(); };
    ports.openApplicationMenu = [this]() { Q_EMIT m_application.openApplicationMenuRequested(); };
    return ports;
}

Actions::ApplicationCommandRouterSessionPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterSessionPorts()
{
    Actions::ApplicationCommandRouterSessionPorts ports;
    ports.openCurrentMediaWith = [this]() {
        if (KiriDocumentSession* session = documentSession()) {
            session->openCurrentMediaWith();
        }
    };
    ports.moveDisplayedFileToTrash
        = [this]() { deleteDisplayedFileByMode(KiriDocumentSession::DeletionMode::MoveToTrash); };
    ports.deleteDisplayedFilePermanently = [this]() {
        deleteDisplayedFileByMode(KiriDocumentSession::DeletionMode::DeletePermanently);
    };
    ports.requestPreviousActiveNavigationWithBoundary
        = [this]() { requestPreviousActiveNavigationWithBoundary(); };
    ports.requestNextActiveNavigationWithBoundary
        = [this]() { requestNextActiveNavigationWithBoundary(); };
    ports.openFirstActiveNavigation = [this]() {
        if (KiriDocumentSession* session = documentSession()) {
            session->openFirstActiveNavigation();
        }
    };
    ports.openLastActiveNavigation = [this]() {
        if (KiriDocumentSession* session = documentSession()) {
            session->openLastActiveNavigation();
        }
    };
    ports.showFirstImageBoundary = [this]() {
        emitCurrentBoundaryText(
            i18nc("@info:status", "First image"), kiriview::NavigationBoundaryEdge::First);
    };
    return ports;
}

Actions::ApplicationCommandRouterImageDocumentPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterImageDocumentPorts()
{
    Actions::ApplicationCommandRouterImageDocumentPorts ports;
    ports.imageAvailable = [this]() { return imageDocument() != nullptr; };
    ports.openPreviousContainer = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->openPreviousContainer();
        }
    };
    ports.openNextContainer = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->openNextContainer();
        }
    };
    ports.openPreviousSinglePage = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->openPreviousSinglePage();
        }
    };
    ports.openNextSinglePage = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->openNextSinglePage();
        }
    };
    ports.rotateClockwise = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->rotateClockwise();
        }
    };
    ports.rotateCounterclockwise = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->rotateCounterclockwise();
        }
    };
    ports.flipHorizontally = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->flipHorizontally();
        }
    };
    ports.flipVertically = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->flipVertically();
        }
    };
    ports.requestToggleTwoPageMode = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestToggleTwoPageMode();
        }
    };
    ports.requestToggleRightToLeftReading = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestToggleRightToLeftReading();
        }
    };
    return ports;
}

Actions::ApplicationCommandRouterImagePresentationPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterImagePresentationPorts()
{
    Actions::ApplicationCommandRouterImagePresentationPorts ports;
    ports.imageViewportHorizontallyPannable = [this]() {
        KiriImageDocument* image = imageDocument();
        return image != nullptr && image->viewportHorizontallyPannable();
    };
    ports.requestViewportPanBy = [this](double dx, double dy) {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestViewportPanBy(dx, dy);
        }
    };
    ports.requestViewportScanForward = [this]() {
        KiriImageDocument* image = imageDocument();
        return image != nullptr && image->requestViewportScanForward();
    };
    ports.requestViewportScanBackward = [this]() {
        KiriImageDocument* image = imageDocument();
        return image != nullptr && image->requestViewportScanBackward();
    };
    ports.requestNextViewportTargetAnchorAtEnd = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestNextViewportTargetAnchorAtEnd();
        }
    };
    ports.requestViewportPanToInitialScanPosition = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestViewportPanToInitialScanPosition();
        }
    };
    ports.requestViewportPanToFinalScanPosition = [this]() {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestViewportPanToFinalScanPosition();
        }
    };
    ports.requestZoomByStepAtCenter = [this](double stepCount) {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestZoomByStepAtCenter(stepCount);
        }
    };
    ports.requestManualZoomPercent = [this](double zoomPercent) {
        if (KiriImageDocument* image = imageDocument()) {
            image->requestManualZoomPercent(zoomPercent);
        }
    };
    ports.requestFitMode = [this]() { requestImageFitMode(KiriImageDocument::ZoomMode::Fit); };
    ports.requestFitHeightMode
        = [this]() { requestImageFitMode(KiriImageDocument::ZoomMode::FitHeight); };
    ports.requestFitWidthMode
        = [this]() { requestImageFitMode(KiriImageDocument::ZoomMode::FitWidth); };
    return ports;
}

Actions::ApplicationCommandRouterPanelPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterPanelPorts()
{
    Actions::ApplicationCommandRouterPanelPorts ports;
    ports.toggleInfoPanel = [this]() { Q_EMIT m_application.toggleInfoPanelRequested(); };
    ports.toggleThumbnailPanel = [this]() { Q_EMIT m_application.toggleThumbnailPanelRequested(); };
    ports.closeInfoPanel = [this]() { Q_EMIT m_application.closeInfoPanelRequested(); };
    return ports;
}

Actions::ApplicationCommandRouterToolbarPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterToolbarPorts()
{
    Actions::ApplicationCommandRouterToolbarPorts ports;
    ports.cancelToolbarTextInputEditing
        = [this]() { Q_EMIT m_application.cancelToolbarTextInputEditingRequested(); };
    return ports;
}

Actions::ApplicationCommandRouterWindowPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterWindowPorts()
{
    Actions::ApplicationCommandRouterWindowPorts ports;
    ports.toggleFullScreen = [this]() {
        if (m_application.m_windowShell != nullptr) {
            m_application.m_windowShell->requestToggleFullscreen();
        }
    };
    ports.leaveFullScreen = [this]() {
        if (m_application.m_windowShell != nullptr) {
            m_application.m_windowShell->requestLeaveFullscreen();
        }
    };
    return ports;
}

Actions::ApplicationCommandRouterHelpPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterHelpPorts()
{
    Actions::ApplicationCommandRouterHelpPorts ports;
    ports.requestShortcutHelp = [this]() { Q_EMIT m_application.shortcutHelpRequested(); };
    ports.requestShortcutConfiguration
        = [this]() { Q_EMIT m_application.shortcutConfigurationRequested(); };
    return ports;
}

Actions::ApplicationCommandRouterVideoPorts
Actions::KiriViewApplicationCommandPortSource::commandRouterVideoPorts()
{
    Actions::ApplicationCommandRouterVideoPorts ports;
    ports.videoAvailable = [this]() { return videoDocument() != nullptr; };
    ports.videoSeekable = [this]() {
        KiriVideoDocument* video = videoDocument();
        return video != nullptr && video->playbackControls()->timelineInteractive();
    };
    ports.videoDuration = [this]() {
        KiriVideoDocument* video = videoDocument();
        return video == nullptr ? qint64(0) : video->playbackControls()->sliderMaximumMsec();
    };
    ports.seekVideoBy = [this](qint64 deltaMilliseconds) {
        if (KiriVideoDocument* video = videoDocument()) {
            video->seekBy(deltaMilliseconds);
        }
    };
    ports.setVideoPosition = [this](qint64 positionMilliseconds) {
        if (KiriVideoDocument* video = videoDocument()) {
            video->setPosition(positionMilliseconds);
        }
    };
    ports.toggleVideoPlayback = [this]() {
        if (KiriVideoDocument* video = videoDocument()) {
            video->togglePlayback();
        }
    };
    return ports;
}

KiriDocumentSession* Actions::KiriViewApplicationCommandPortSource::documentSession() const
{
    return m_application.m_documentSession.data();
}

KiriImageDocument* Actions::KiriViewApplicationCommandPortSource::imageDocument() const
{
    KiriDocumentSession* session = documentSession();
    return session == nullptr ? nullptr : session->imageDocument();
}

KiriVideoDocument* Actions::KiriViewApplicationCommandPortSource::videoDocument() const
{
    KiriDocumentSession* session = documentSession();
    return session == nullptr ? nullptr : session->videoDocument();
}

void Actions::KiriViewApplicationCommandPortSource::deleteDisplayedFileByMode(
    KiriDocumentSession::DeletionMode mode)
{
    if (KiriDocumentSession* session = documentSession()) {
        session->deleteDisplayedFile(mode);
    }
}

void Actions::KiriViewApplicationCommandPortSource::requestImageFitMode(
    KiriImageDocument::ZoomMode mode)
{
    if (KiriImageDocument* image = imageDocument()) {
        image->requestFitMode(mode);
    }
}

void Actions::KiriViewApplicationCommandPortSource::requestPreviousActiveNavigationWithBoundary()
{
    const QPointer<KiriDocumentSession> originSession = documentSession();
    if (originSession == nullptr) {
        return;
    }
    const std::optional<kiriview::NavigationBoundaryCorrelation> correlation
        = kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First,
            kiriview::activeNavigationBoundaryFacts(originSession));
    const QString message = originSession->requestPreviousActiveNavigationBoundaryText();
    if (correlation.has_value() && originSession != nullptr) {
        emitBoundaryText(message, *correlation, originSession);
    }
}

void Actions::KiriViewApplicationCommandPortSource::requestNextActiveNavigationWithBoundary()
{
    const QPointer<KiriDocumentSession> originSession = documentSession();
    if (originSession == nullptr) {
        return;
    }
    const std::optional<kiriview::NavigationBoundaryCorrelation> correlation
        = kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::Last,
            kiriview::activeNavigationBoundaryFacts(originSession));
    const QString message = originSession->requestNextActiveNavigationBoundaryText();
    if (correlation.has_value() && originSession != nullptr) {
        emitBoundaryText(message, *correlation, originSession);
    }
}

void Actions::KiriViewApplicationCommandPortSource::emitCurrentBoundaryText(
    const QString& message, kiriview::NavigationBoundaryEdge edge)
{
    const QPointer<KiriDocumentSession> originSession = documentSession();
    const std::optional<kiriview::NavigationBoundaryCorrelation> correlation
        = kiriview::correlateNavigationBoundary(
            edge, kiriview::activeNavigationBoundaryFacts(originSession));
    if (correlation.has_value() && originSession != nullptr) {
        emitBoundaryText(message, *correlation, originSession);
    }
}

void Actions::KiriViewApplicationCommandPortSource::emitBoundaryText(const QString& message,
    const kiriview::NavigationBoundaryCorrelation& correlation, KiriDocumentSession* originSession)
{
    if (message.isEmpty() || originSession == nullptr || originSession != documentSession()
        || !kiriview::navigationBoundaryCorrelationIsCurrent(
            correlation, kiriview::activeNavigationBoundaryFacts(originSession))) {
        return;
    }
    Q_EMIT m_application.imageBoundaryReached(message, correlation, originSession);
}
