// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationshortcutpolicy.h"
#include "application/kiriviewapplicationactions.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiriviewapplication.h"
#include "facade/kiriwindowshell.h"
#include "kiriviewstate.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KirigamiActionCollection>
#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QTest>
#include <QVariantList>
#include <array>
#include <cstddef>

namespace {
namespace Actions = kiriview::ApplicationActions;

using DomainActionId = kiriview::ApplicationActions::ActionId;

struct ActionIdMapping
{
    KiriViewApplication::ActionId facade;
    DomainActionId domain;
};

constexpr std::array actionIdMappings {
    ActionIdMapping { KiriViewApplication::FileOpenAction, DomainActionId::FileOpenAction },
    ActionIdMapping { KiriViewApplication::FileOpenWithAction, DomainActionId::FileOpenWithAction },
    ActionIdMapping {
        KiriViewApplication::FileMoveToTrashAction, DomainActionId::FileMoveToTrashAction },
    ActionIdMapping { KiriViewApplication::FileDeleteAction, DomainActionId::FileDeleteAction },
    ActionIdMapping {
        KiriViewApplication::GoPreviousArchiveAction, DomainActionId::GoPreviousArchiveAction },
    ActionIdMapping {
        KiriViewApplication::GoNextArchiveAction, DomainActionId::GoNextArchiveAction },
    ActionIdMapping {
        KiriViewApplication::GoPreviousImageAction, DomainActionId::GoPreviousImageAction },
    ActionIdMapping { KiriViewApplication::GoNextImageAction, DomainActionId::GoNextImageAction },
    ActionIdMapping { KiriViewApplication::GoFirstImageAction, DomainActionId::GoFirstImageAction },
    ActionIdMapping { KiriViewApplication::GoLastImageAction, DomainActionId::GoLastImageAction },
    ActionIdMapping { KiriViewApplication::ViewZoomInAction, DomainActionId::ViewZoomInAction },
    ActionIdMapping { KiriViewApplication::ViewZoomOutAction, DomainActionId::ViewZoomOutAction },
    ActionIdMapping {
        KiriViewApplication::ViewZoom50PercentAction, DomainActionId::ViewZoom50PercentAction },
    ActionIdMapping {
        KiriViewApplication::ViewZoom100PercentAction, DomainActionId::ViewZoom100PercentAction },
    ActionIdMapping {
        KiriViewApplication::ViewZoom200PercentAction, DomainActionId::ViewZoom200PercentAction },
    ActionIdMapping { KiriViewApplication::ViewFitAction, DomainActionId::ViewFitAction },
    ActionIdMapping {
        KiriViewApplication::ViewFitHeightAction, DomainActionId::ViewFitHeightAction },
    ActionIdMapping { KiriViewApplication::ViewFitWidthAction, DomainActionId::ViewFitWidthAction },
    ActionIdMapping {
        KiriViewApplication::ViewRotateClockwiseAction, DomainActionId::ViewRotateClockwiseAction },
    ActionIdMapping { KiriViewApplication::ViewRotateCounterclockwiseAction,
        DomainActionId::ViewRotateCounterclockwiseAction },
    ActionIdMapping { KiriViewApplication::ViewToggleTwoPageModeAction,
        DomainActionId::ViewToggleTwoPageModeAction },
    ActionIdMapping { KiriViewApplication::ViewToggleRightToLeftReadingAction,
        DomainActionId::ViewToggleRightToLeftReadingAction },
    ActionIdMapping {
        KiriViewApplication::ViewToggleInfoPanelAction, DomainActionId::ViewToggleInfoPanelAction },
    ActionIdMapping { KiriViewApplication::ViewToggleThumbnailPanelAction,
        DomainActionId::ViewToggleThumbnailPanelAction },
    ActionIdMapping { KiriViewApplication::ViewGoToContentStartAction,
        DomainActionId::ViewGoToContentStartAction },
    ActionIdMapping {
        KiriViewApplication::ViewGoToContentEndAction, DomainActionId::ViewGoToContentEndAction },
    ActionIdMapping {
        KiriViewApplication::ViewScanForwardAction, DomainActionId::ViewScanForwardAction },
    ActionIdMapping {
        KiriViewApplication::ViewScanBackwardAction, DomainActionId::ViewScanBackwardAction },
    ActionIdMapping { KiriViewApplication::ViewToggleVideoPlaybackAction,
        DomainActionId::ViewToggleVideoPlaybackAction },
    ActionIdMapping {
        KiriViewApplication::WindowFullscreenAction, DomainActionId::WindowFullscreenAction },
    ActionIdMapping {
        KiriViewApplication::HelpShortcutsAction, DomainActionId::HelpShortcutsAction },
    ActionIdMapping { KiriViewApplication::OptionsConfigureKeybindingAction,
        DomainActionId::OptionsConfigureKeybindingAction },
    ActionIdMapping {
        KiriViewApplication::OptionsShowMenubarAction, DomainActionId::OptionsShowMenubarAction },
    ActionIdMapping {
        KiriViewApplication::OpenApplicationMenuAction, DomainActionId::OpenApplicationMenuAction },
    ActionIdMapping { KiriViewApplication::FileQuitAction, DomainActionId::FileQuitAction },
};

static_assert(actionIdMappings.size() == Actions::actionDefinitionCount);

constexpr const char* interfaceConfigGroup = "Interface";
constexpr const char* menuPresentationConfigKey = "menuPresentation";
constexpr const char* stateConfigFileName = "kiriviewstaterc";
constexpr int shortcutHelpActionIdRole = Qt::UserRole + 1;
constexpr int shortcutHelpActionNameRole = Qt::UserRole + 2;
constexpr int shortcutHelpActionTextRole = Qt::UserRole + 3;
constexpr int shortcutHelpShortcutTextRole = Qt::UserRole + 4;
constexpr int shortcutHelpCategoryKeyRole = Qt::UserRole + 5;
constexpr int shortcutHelpCategoryTextRole = Qt::UserRole + 6;
constexpr int shortcutHelpCategoryFirstRole = Qt::UserRole + 7;
constexpr int shortcutHelpCategoryLastRole = Qt::UserRole + 8;
constexpr int shortcutHelpShortcutKeyTextsRole = Qt::UserRole + 9;
constexpr int shortcutHelpScopeTextRole = Qt::UserRole + 10;

QKeySequence shortcut(const QString& sequence)
{
    return QKeySequence::fromString(sequence, QKeySequence::PortableText);
}

QString nativeText(const QKeySequence& sequence)
{
    return sequence.toString(QKeySequence::NativeText);
}

QString definitionActionName(const Actions::ActionDefinition& definition)
{
    return QString::fromLatin1(definition.name);
}

bool actionDefaultShortcutsAreManagedByKiriView(const Actions::ActionDefinition& definition)
{
    return definition.kind != Actions::RegistrationKind::Inherited;
}

KiriViewApplication::ActionId facadeActionId(DomainActionId actionId)
{
    return KiriViewApplication::facadeActionId(actionId);
}

KConfigGroup stateInterfaceGroup()
{
    return KConfigGroup(KiriViewState::self()->config(), QLatin1String(interfaceConfigGroup));
}

void resetConfig()
{
    KiriViewState* state = KiriViewState::self();
    state->config()->deleteGroup(QStringLiteral("Interface"));
    state->config()->sync();
    state->config()->reparseConfiguration();
    state->read();

    KSharedConfig::Ptr appConfig = KSharedConfig::openConfig();
    appConfig->deleteGroup(QStringLiteral("Shortcuts"));
    appConfig->deleteGroup(QStringLiteral("ViewerLocalShortcuts"));
    appConfig->sync();
    appConfig->reparseConfiguration();
}

void expectDefaultShortcuts(KiriViewApplication& application,
    KiriViewApplication::ActionId actionId, const QList<QKeySequence>& shortcuts)
{
    QAction* action = application.actionForId(actionId);
    QVERIFY(action != nullptr);
    QCOMPARE(KirigamiActionCollection::defaultShortcuts(action), shortcuts);
    QCOMPARE(action->shortcuts(), shortcuts);
}

QModelIndex shortcutHelpIndexForAction(QAbstractItemModel* model, const QString& actionName)
{
    if (model == nullptr) {
        return {};
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (model->data(index, shortcutHelpActionNameRole).toString() == actionName) {
            return index;
        }
    }

    return {};
}

QModelIndex shortcutHelpIndexForActionAndScope(
    QAbstractItemModel* model, const QString& actionName, const QString& scopeText)
{
    if (model == nullptr) {
        return {};
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (model->data(index, shortcutHelpActionNameRole).toString() == actionName
            && model->data(index, shortcutHelpScopeTextRole).toString() == scopeText) {
            return index;
        }
    }

    return {};
}
}

class TestKiriViewApplication : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void actionsAreRegisteredWithDefaultShortcuts();
    void actionDefinitionTableIsCanonicalIdentitySource();
    void facadeActionIdsConvertAtApplicationBoundary();
    void actionIdsResolveActionsAndShortcutScopes();
    void navigationPresentationApiConvertsRuntimeProjection();
    void typedShortcutApisReturnCurrentShortcuts();
    void zoomPresetActionsUseNewDefaultShortcutMap();
    void shortcutScopeApisSeparateProgramWideAndViewerLocalShortcuts();
    void menuShortcutTextReturnsFirstDisplaySafeShortcut();
    void shortcutRevisionTracksShortcutChanges();
    void fixedCommandShortcutsAreNonConfigurable();
    void shortcutHelpModelListsConfigurableActions();
    void shortcutHelpModelUpdatesShortcutText();
    void shortcutHelpModelResetsWhenConfigurableRowsChange();
    void menuPresentationDefaultsToHamburgerMenu();
    void invalidStoredMenuPresentationFallsBackToHamburgerMenu();
    void menuPresentationPersists();
    void menuPresentationStateUsesGenericStateLocation();
    void showMenubarActionTogglesMenuPresentation();
    void windowShellApplicationAttachmentIsIdentitySafe();
    void windowShellDocumentSessionAttachmentIsIdentitySafe();
    void windowShellRefreshesTitleWhenSessionDetachesOrIsDestroyed();
};

void TestKiriViewApplication::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    resetConfig();
}

void TestKiriViewApplication::init() { resetConfig(); }

void TestKiriViewApplication::cleanup() { resetConfig(); }

void TestKiriViewApplication::actionsAreRegisteredWithDefaultShortcuts()
{
    KiriViewApplication application;

    for (const Actions::ActionDefinition& definition : Actions::definitions()) {
        const auto actionId = facadeActionId(definition.actionId);
        QVERIFY(application.actionForId(actionId) != nullptr);
        if (actionDefaultShortcutsAreManagedByKiriView(definition)) {
            expectDefaultShortcuts(application, actionId,
                Actions::defaultShortcuts(definition.defaultProgramWideShortcuts));
        }
    }
}

void TestKiriViewApplication::actionDefinitionTableIsCanonicalIdentitySource()
{
    const auto& definitions = Actions::definitions();
    QCOMPARE(definitions.size(), Actions::actionDefinitionCount);

    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto actionId = static_cast<DomainActionId>(index);
        const Actions::ActionDefinition* definition = Actions::definitionForId(actionId);

        QVERIFY(definition != nullptr);
        QCOMPARE(definition, &definitions.at(index));
        QCOMPARE(definition->actionId, actionId);
    }

    QVERIFY(Actions::definitionForId(static_cast<DomainActionId>(-1)) == nullptr);
    QVERIFY(Actions::definitionForId(DomainActionId::ActionCount) == nullptr);
}

void TestKiriViewApplication::facadeActionIdsConvertAtApplicationBoundary()
{
    for (const ActionIdMapping& mapping : actionIdMappings) {
        QCOMPARE(KiriViewApplication::domainActionId(mapping.facade), mapping.domain);
        QCOMPARE(KiriViewApplication::facadeActionId(mapping.domain), mapping.facade);
    }

    QCOMPARE(KiriViewApplication::domainActionId(KiriViewApplication::ActionCount),
        DomainActionId::ActionCount);
    QCOMPARE(KiriViewApplication::domainActionId(static_cast<KiriViewApplication::ActionId>(-1)),
        DomainActionId::ActionCount);
    QCOMPARE(KiriViewApplication::domainActionId(static_cast<KiriViewApplication::ActionId>(999)),
        DomainActionId::ActionCount);
    QCOMPARE(KiriViewApplication::facadeActionId(DomainActionId::ActionCount),
        KiriViewApplication::ActionCount);
    QCOMPARE(KiriViewApplication::facadeActionId(static_cast<DomainActionId>(-1)),
        KiriViewApplication::ActionCount);
    QCOMPARE(KiriViewApplication::facadeActionId(static_cast<DomainActionId>(999)),
        KiriViewApplication::ActionCount);

    QCOMPARE(KiriViewApplication::domainMenuPresentation(KiriViewApplication::MenuBar),
        Actions::MenuPresentation::MenuBar);
    QCOMPARE(KiriViewApplication::facadeMenuPresentation(Actions::MenuPresentation::HamburgerMenu),
        KiriViewApplication::HamburgerMenu);
    QCOMPARE(KiriViewApplication::domainMenuPresentation(
                 static_cast<KiriViewApplication::MenuPresentation>(99)),
        Actions::MenuPresentation::HamburgerMenu);
}

void TestKiriViewApplication::actionIdsResolveActionsAndShortcutScopes()
{
    KiriViewApplication application;

    for (const Actions::ActionDefinition& definition : Actions::definitions()) {
        const KiriViewApplication::ActionId actionId = facadeActionId(definition.actionId);
        QAction* action = application.actionForId(actionId);
        QVERIFY(action != nullptr);
        const QList<QKeySequence> programWide = application.programWideShortcutsForId(actionId);
        QCOMPARE(action->shortcuts(), programWide);
        QCOMPARE(application.menuShortcutTextForId(actionId),
            Actions::menuShortcut(programWide).toString(QKeySequence::NativeText));
    }

    const auto invalidActionId = static_cast<KiriViewApplication::ActionId>(-1);
    QCOMPARE(application.actionForId(invalidActionId), nullptr);
    QCOMPARE(application.programWideShortcutsForId(invalidActionId), QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(invalidActionId), QList<QKeySequence>());
    QVERIFY(application.menuShortcutTextForId(invalidActionId).isEmpty());

    const auto outOfRangeActionId = static_cast<KiriViewApplication::ActionId>(999);
    QCOMPARE(application.actionForId(outOfRangeActionId), nullptr);
    QCOMPARE(application.programWideShortcutsForId(outOfRangeActionId), QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(outOfRangeActionId), QList<QKeySequence>());
    QVERIFY(application.menuShortcutTextForId(outOfRangeActionId).isEmpty());

    QCOMPARE(application.actionForId(KiriViewApplication::ActionCount), nullptr);
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::ActionCount),
        QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ActionCount),
        QList<QKeySequence>());
    QVERIFY(application.menuShortcutTextForId(KiriViewApplication::ActionCount).isEmpty());
}

void TestKiriViewApplication::navigationPresentationApiConvertsRuntimeProjection()
{
    KiriViewApplication application;

    QCOMPARE(
        application.navigationPresentationActionId(KiriViewApplication::LeadingImageActionSlot),
        KiriViewApplication::GoPreviousImageAction);
    QCOMPARE(
        application.navigationPresentationIconActionId(KiriViewApplication::LeadingImageActionSlot),
        KiriViewApplication::GoPreviousImageAction);
    QCOMPARE(
        application.navigationPresentationActionId(KiriViewApplication::TrailingImageActionSlot),
        KiriViewApplication::GoNextImageAction);
    QCOMPARE(application.navigationPresentationIconActionId(
                 KiriViewApplication::TrailingImageActionSlot),
        KiriViewApplication::GoNextImageAction);
    QCOMPARE(
        application.navigationPresentationActionId(KiriViewApplication::FirstImageMenuActionSlot),
        KiriViewApplication::GoFirstImageAction);
    QCOMPARE(application.navigationPresentationIconActionId(
                 KiriViewApplication::FirstImageMenuActionSlot),
        KiriViewApplication::GoFirstImageAction);
    QCOMPARE(application.navigationPresentationActionId(
                 KiriViewApplication::LeadingArchiveMenuActionSlot),
        KiriViewApplication::GoPreviousArchiveAction);
    QCOMPARE(application.navigationPresentationIconActionId(
                 KiriViewApplication::LeadingArchiveMenuActionSlot),
        KiriViewApplication::GoPreviousArchiveAction);
    QCOMPARE(application.navigationApplicationMenuActionIds(),
        QVariantList({ static_cast<int>(KiriViewApplication::GoPreviousArchiveAction),
            static_cast<int>(KiriViewApplication::GoNextArchiveAction) }));

    QCOMPARE(application.navigationPresentationActionId(
                 static_cast<KiriViewApplication::NavigationPresentationSlot>(-1)),
        KiriViewApplication::ActionCount);
    QCOMPARE(application.navigationPresentationIconActionId(
                 static_cast<KiriViewApplication::NavigationPresentationSlot>(999)),
        KiriViewApplication::ActionCount);
}

void TestKiriViewApplication::typedShortcutApisReturnCurrentShortcuts()
{
    KiriViewApplication application;

    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewZoomInAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("=")), shortcut(QStringLiteral("+")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewZoomOutAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("-")) }));
    QCOMPARE(
        application.viewerLocalShortcutsForId(KiriViewApplication::ViewToggleTwoPageModeAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("S")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(
                 KiriViewApplication::ViewToggleRightToLeftReadingAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("B")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewToggleInfoPanelAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("I")) }));
    QCOMPARE(
        application.viewerLocalShortcutsForId(KiriViewApplication::ViewToggleThumbnailPanelAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("T")) }));
    QCOMPARE(
        application.viewerLocalShortcutsForId(KiriViewApplication::ViewToggleVideoPlaybackAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("P")) }));
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::OptionsShowMenubarAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+M")) }));
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::OpenApplicationMenuAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("F10")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("R")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(
                 KiriViewApplication::ViewRotateCounterclockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Shift+R")) }));

    QAction* openAction = application.actionForId(KiriViewApplication::FileOpenAction);
    QVERIFY(openAction != nullptr);
    const QList<QKeySequence> customShortcuts = {
        shortcut(QStringLiteral("Alt+O")),
        shortcut(QStringLiteral("Ctrl+Shift+O")),
    };

    openAction->setShortcuts(customShortcuts);

    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::FileOpenAction),
        customShortcuts);

    openAction->setShortcuts({ shortcut(QStringLiteral("O")) });
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::FileOpenAction),
        QList<QKeySequence>());
}

void TestKiriViewApplication::zoomPresetActionsUseNewDefaultShortcutMap()
{
    KiriViewApplication application;

    expectDefaultShortcuts(application, KiriViewApplication::ViewZoom50PercentAction, {});
    expectDefaultShortcuts(application, KiriViewApplication::ViewZoom100PercentAction, {});
    expectDefaultShortcuts(application, KiriViewApplication::ViewZoom200PercentAction, {});
    expectDefaultShortcuts(application, KiriViewApplication::ViewFitHeightAction, {});
    expectDefaultShortcuts(application, KiriViewApplication::ViewFitWidthAction, {});
    expectDefaultShortcuts(application, KiriViewApplication::ViewFitAction, {});
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewZoom50PercentAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("`")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewZoom100PercentAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("1")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewZoom200PercentAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("2")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewFitHeightAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("8")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewFitWidthAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("9")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewFitAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("0")) }));
    QCOMPARE(
        application.viewerLocalShortcutsForId(KiriViewApplication::ViewToggleVideoPlaybackAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("P")) }));
}

void TestKiriViewApplication::shortcutScopeApisSeparateProgramWideAndViewerLocalShortcuts()
{
    KiriViewApplication application;

    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::FileQuitAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Q")) }));
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::FileQuitAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+Q")) }));
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("R")) }));
    QCOMPARE(application.programWideShortcutsForId(
                 KiriViewApplication::ViewRotateCounterclockwiseAction),
        QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(
                 KiriViewApplication::ViewRotateCounterclockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Shift+R")) }));
    QAction* quitAction = application.actionForId(KiriViewApplication::FileQuitAction);
    QVERIFY(quitAction != nullptr);
    quitAction->setShortcuts({ shortcut(QStringLiteral("Alt+Q")),
        shortcut(QStringLiteral("Shift+Q")), shortcut(QStringLiteral("Meta+Q")),
        shortcut(QStringLiteral("Ctrl+Shift+Q")), shortcut(QStringLiteral("Q")) });

    QCOMPARE(quitAction->shortcuts(),
        QList<QKeySequence>(
            { shortcut(QStringLiteral("Alt+Q")), shortcut(QStringLiteral("Shift+Q")),
                shortcut(QStringLiteral("Meta+Q")), shortcut(QStringLiteral("Ctrl+Shift+Q")) }));
    QCOMPARE(application.programWideShortcutsForId(KiriViewApplication::FileQuitAction),
        QList<QKeySequence>(
            { shortcut(QStringLiteral("Alt+Q")), shortcut(QStringLiteral("Shift+Q")),
                shortcut(QStringLiteral("Meta+Q")), shortcut(QStringLiteral("Ctrl+Shift+Q")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::FileQuitAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Q")) }));

    QVERIFY(application.setViewerLocalShortcutsForId(
        KiriViewApplication::ViewRotateClockwiseAction, { shortcut(QStringLiteral("Ctrl+L")) }));
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+L")) }));
    QCOMPARE(application.menuShortcutTextForId(KiriViewApplication::ViewRotateClockwiseAction),
        QString());
}

void TestKiriViewApplication::menuShortcutTextReturnsFirstDisplaySafeShortcut()
{
    KiriViewApplication application;

    QVERIFY(application.menuShortcutTextForId(KiriViewApplication::ViewRotateClockwiseAction)
            .isEmpty());
    QVERIFY(
        application.menuShortcutTextForId(KiriViewApplication::FileMoveToTrashAction).isEmpty());
    QVERIFY(application.menuShortcutTextForId(KiriViewApplication::FileDeleteAction).isEmpty());
    QVERIFY(
        application.menuShortcutTextForId(KiriViewApplication::GoPreviousImageAction).isEmpty());

    QAction* openAction = application.actionForId(KiriViewApplication::FileOpenAction);
    QVERIFY(openAction != nullptr);
    openAction->setShortcuts({ QKeySequence(), shortcut(QStringLiteral("Delete")),
        shortcut(QStringLiteral("Alt+O")), shortcut(QStringLiteral("Ctrl+Shift+O")) });

    QCOMPARE(application.menuShortcutTextForId(KiriViewApplication::FileOpenAction),
        nativeText(shortcut(QStringLiteral("Alt+O"))));
    QCOMPARE(application.menuShortcutTextForId(KiriViewApplication::OptionsShowMenubarAction),
        nativeText(shortcut(QStringLiteral("Ctrl+M"))));
    QCOMPARE(application.menuShortcutTextForId(KiriViewApplication::OpenApplicationMenuAction),
        nativeText(shortcut(QStringLiteral("F10"))));
    QVERIFY(application.menuShortcutTextForId(static_cast<KiriViewApplication::ActionId>(999))
            .isEmpty());
}

void TestKiriViewApplication::shortcutRevisionTracksShortcutChanges()
{
    KiriViewApplication application;
    QSignalSpy revisionSpy(&application, &KiriViewApplication::shortcutRevisionChanged);
    QAction* rotateAction = application.actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QVERIFY(rotateAction != nullptr);

    const int initialRevision = application.shortcutRevision();

    QVERIFY(application.setViewerLocalShortcutsForId(
        KiriViewApplication::ViewRotateClockwiseAction, { shortcut(QStringLiteral("Ctrl+L")) }));

    QTRY_COMPARE(revisionSpy.count(), 1);
    QCOMPARE(application.shortcutRevision(), initialRevision + 1);
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+L")) }));

    QVERIFY(application.setViewerLocalShortcutsForId(
        KiriViewApplication::ViewRotateClockwiseAction, { shortcut(QStringLiteral("R")) }));

    QTRY_COMPARE(revisionSpy.count(), 2);
    QCOMPARE(application.shortcutRevision(), initialRevision + 2);
    QCOMPARE(rotateAction->shortcuts(), QList<QKeySequence>());
    QCOMPARE(application.viewerLocalShortcutsForId(KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("R")) }));
}

void TestKiriViewApplication::fixedCommandShortcutsAreNonConfigurable()
{
    KiriViewApplication application;

    QAction* showMenubarAction
        = application.actionForId(KiriViewApplication::OptionsShowMenubarAction);
    QVERIFY(showMenubarAction != nullptr);
    QCOMPARE(KirigamiActionCollection::defaultShortcuts(showMenubarAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+M")) }));
    QCOMPARE(showMenubarAction->shortcuts(),
        QList<QKeySequence>({ shortcut(QStringLiteral("Ctrl+M")) }));
    QVERIFY(!KirigamiActionCollection::isShortcutsConfigurable(showMenubarAction));

    QAction* openApplicationMenuAction
        = application.actionForId(KiriViewApplication::OpenApplicationMenuAction);
    QVERIFY(openApplicationMenuAction != nullptr);
    QCOMPARE(KirigamiActionCollection::defaultShortcuts(openApplicationMenuAction),
        QList<QKeySequence>({ shortcut(QStringLiteral("F10")) }));
    QCOMPARE(openApplicationMenuAction->shortcuts(),
        QList<QKeySequence>({ shortcut(QStringLiteral("F10")) }));
    QVERIFY(!KirigamiActionCollection::isShortcutsConfigurable(openApplicationMenuAction));
}

void TestKiriViewApplication::shortcutHelpModelListsConfigurableActions()
{
    KiriViewApplication application;

    QAbstractItemModel* model = application.shortcutHelpModel();
    QVERIFY(model != nullptr);

    const QModelIndex moveToTrashIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("movetotrash"));
    QVERIFY(moveToTrashIndex.isValid());
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpActionIdRole).toInt(),
        static_cast<int>(KiriViewApplication::FileMoveToTrashAction));
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpActionTextRole).toString(),
        QStringLiteral("Move to Trash"));
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpShortcutTextRole).toString(),
        nativeText(shortcut(QStringLiteral("Delete"))));
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("file"));
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpCategoryTextRole).toString(),
        QStringLiteral("File"));
    QCOMPARE(model->data(moveToTrashIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("Delete"))) }));

    const QModelIndex deleteIndex = shortcutHelpIndexForAction(model, QStringLiteral("deletefile"));
    QVERIFY(deleteIndex.isValid());
    QCOMPARE(model->data(deleteIndex, shortcutHelpActionTextRole).toString(),
        QStringLiteral("Delete Permanently"));

    const QModelIndex quitProgramWideIndex = shortcutHelpIndexForActionAndScope(
        model, QStringLiteral("file_quit"), QStringLiteral("Program-wide"));
    QVERIFY(quitProgramWideIndex.isValid());
    QCOMPARE(model->data(quitProgramWideIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("Ctrl+Q"))) }));

    const QModelIndex quitViewerLocalIndex = shortcutHelpIndexForActionAndScope(
        model, QStringLiteral("file_quit"), QStringLiteral("Viewer-local"));
    QVERIFY(quitViewerLocalIndex.isValid());
    QCOMPARE(model->data(quitViewerLocalIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("Q"))) }));

    const QModelIndex openIndex = shortcutHelpIndexForAction(model, QStringLiteral("file_open"));
    QVERIFY(openIndex.isValid());
    QVERIFY(model->data(openIndex, shortcutHelpCategoryFirstRole).toBool());

    const QModelIndex navigationIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("go_previous_archive"));
    QVERIFY(navigationIndex.isValid());
    QCOMPARE(model->data(navigationIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("navigation"));
    QCOMPARE(model->data(navigationIndex, shortcutHelpCategoryTextRole).toString(),
        QStringLiteral("Navigation"));
    QVERIFY(model->data(navigationIndex, shortcutHelpCategoryFirstRole).toBool());

    const QModelIndex viewIndex = shortcutHelpIndexForAction(model, QStringLiteral("view_zoom_in"));
    QVERIFY(viewIndex.isValid());
    QCOMPARE(
        model->data(viewIndex, shortcutHelpCategoryKeyRole).toString(), QStringLiteral("view"));
    QVERIFY(model->data(viewIndex, shortcutHelpCategoryFirstRole).toBool());

    const QModelIndex zoomPresetIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("view_zoom_100_percent"));
    QVERIFY(zoomPresetIndex.isValid());
    QCOMPARE(model->data(zoomPresetIndex, shortcutHelpActionTextRole).toString(),
        QStringLiteral("Zoom to 100%"));
    QCOMPARE(model->data(zoomPresetIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("1"))) }));
    QCOMPARE(model->data(zoomPresetIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("view"));

    const QModelIndex videoPlaybackIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("view_toggle_video_playback"));
    QVERIFY(videoPlaybackIndex.isValid());
    QCOMPARE(model->data(videoPlaybackIndex, shortcutHelpActionTextRole).toString(),
        QStringLiteral("Play/Pause"));
    QCOMPARE(model->data(videoPlaybackIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("P"))) }));
    QCOMPARE(model->data(videoPlaybackIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("view"));

    const QModelIndex panelsIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("view_toggle_info_panel"));
    QVERIFY(panelsIndex.isValid());
    QCOMPARE(
        model->data(panelsIndex, shortcutHelpCategoryKeyRole).toString(), QStringLiteral("panels"));
    QVERIFY(model->data(panelsIndex, shortcutHelpCategoryFirstRole).toBool());

    const QModelIndex fullscreenIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("window_fullscreen"));
    QVERIFY(fullscreenIndex.isValid());
    QCOMPARE(model->data(fullscreenIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("window"));
    QCOMPARE(model->data(fullscreenIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("Ctrl+F"))),
            nativeText(shortcut(QStringLiteral("F11"))) }));

    const QModelIndex configureIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("options_configure_keybinding"));
    QVERIFY(configureIndex.isValid());
    QCOMPARE(model->data(configureIndex, shortcutHelpCategoryKeyRole).toString(),
        QStringLiteral("settings"));
    QVERIFY(model->data(configureIndex, shortcutHelpCategoryFirstRole).toBool());

    const QModelIndex helpIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("help_shortcuts"));
    QVERIFY(helpIndex.isValid());
    QCOMPARE(
        model->data(helpIndex, shortcutHelpCategoryKeyRole).toString(), QStringLiteral("help"));
    QVERIFY(model->data(helpIndex, shortcutHelpCategoryFirstRole).toBool());

    QVERIFY(
        !shortcutHelpIndexForAction(model, QStringLiteral("go_previous_single_page")).isValid());
    QVERIFY(!shortcutHelpIndexForAction(model, QStringLiteral("go_next_single_page")).isValid());
    QVERIFY(!shortcutHelpIndexForAction(model, QStringLiteral("options_show_menubar")).isValid());
    QVERIFY(!shortcutHelpIndexForAction(model, QStringLiteral("open_application_menu")).isValid());
}

void TestKiriViewApplication::shortcutHelpModelUpdatesShortcutText()
{
    KiriViewApplication application;

    QAbstractItemModel* model = application.shortcutHelpModel();
    QVERIFY(model != nullptr);
    const QModelIndex rotateIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("view_rotate_clockwise"));
    QVERIFY(rotateIndex.isValid());
    QCOMPARE(model->data(rotateIndex, shortcutHelpShortcutTextRole).toString(),
        nativeText(shortcut(QStringLiteral("R"))));
    QCOMPARE(model->data(rotateIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("R"))) }));

    QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);

    QVERIFY(application.setViewerLocalShortcutsForId(
        KiriViewApplication::ViewRotateClockwiseAction, { shortcut(QStringLiteral("Ctrl+L")) }));

    QTRY_VERIFY(!dataChangedSpy.isEmpty());
    QCOMPARE(model->data(rotateIndex, shortcutHelpShortcutTextRole).toString(),
        nativeText(shortcut(QStringLiteral("Ctrl+L"))));
    QCOMPARE(model->data(rotateIndex, shortcutHelpShortcutKeyTextsRole).toStringList(),
        QStringList({ nativeText(shortcut(QStringLiteral("Ctrl+L"))) }));
}

void TestKiriViewApplication::shortcutHelpModelResetsWhenConfigurableRowsChange()
{
    KiriViewApplication application;

    QAbstractItemModel* model = application.shortcutHelpModel();
    QVERIFY(model != nullptr);
    const QModelIndex rotateIndex
        = shortcutHelpIndexForAction(model, QStringLiteral("view_rotate_clockwise"));
    QVERIFY(rotateIndex.isValid());

    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
    QAction* rotateAction = application.actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QVERIFY(rotateAction != nullptr);

    KirigamiActionCollection::setShortcutsConfigurable(rotateAction, false);
    rotateAction->changed();

    QTRY_COMPARE(resetSpy.count(), 1);
    QVERIFY(!shortcutHelpIndexForAction(model, QStringLiteral("view_rotate_clockwise")).isValid());
}

void TestKiriViewApplication::menuPresentationDefaultsToHamburgerMenu()
{
    KiriViewApplication application;

    QCOMPARE(KiriViewState::defaultMenuPresentationValue(),
        static_cast<int>(KiriViewState::EnumMenuPresentation::HamburgerMenu));
    QCOMPARE(application.menuPresentation(), KiriViewApplication::HamburgerMenu);
    QCOMPARE(KiriViewState::menuPresentation(),
        static_cast<int>(KiriViewState::EnumMenuPresentation::HamburgerMenu));
    QVERIFY(!stateInterfaceGroup().hasKey(QLatin1String(menuPresentationConfigKey)));
}

void TestKiriViewApplication::invalidStoredMenuPresentationFallsBackToHamburgerMenu()
{
    KiriViewState::setMenuPresentation(99);

    KiriViewApplication application;

    QCOMPARE(application.menuPresentation(), KiriViewApplication::HamburgerMenu);
}

void TestKiriViewApplication::menuPresentationPersists()
{
    {
        KiriViewApplication application;
        QSignalSpy changedSpy(&application, &KiriViewApplication::menuPresentationChanged);

        application.setMenuPresentation(KiriViewApplication::MenuBar);

        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(application.menuPresentation(), KiriViewApplication::MenuBar);
        QCOMPARE(KiriViewState::menuPresentation(),
            static_cast<int>(KiriViewState::EnumMenuPresentation::MenuBar));
        QCOMPARE(
            stateInterfaceGroup().readEntry(QLatin1String(menuPresentationConfigKey), QString()),
            QStringLiteral("MenuBar"));

        KiriViewState::setMenuPresentation(KiriViewState::EnumMenuPresentation::HamburgerMenu);
        QCOMPARE(application.menuPresentation(), KiriViewApplication::MenuBar);
    }

    KiriViewState::self()->read();
    KiriViewApplication reloadedApplication;
    QCOMPARE(reloadedApplication.menuPresentation(), KiriViewApplication::MenuBar);
}

void TestKiriViewApplication::menuPresentationStateUsesGenericStateLocation()
{
    KiriViewState::setMenuPresentation(KiriViewState::EnumMenuPresentation::MenuBar);
    KiriViewState::self()->save();

    const QString statePath = QStandardPaths::locate(
        QStandardPaths::GenericStateLocation, QLatin1String(stateConfigFileName));
    QVERIFY(!statePath.isEmpty());
    QCOMPARE(QFileInfo(statePath).absolutePath(),
        QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation));
}

void TestKiriViewApplication::showMenubarActionTogglesMenuPresentation()
{
    KiriViewApplication application;

    QAction* showMenubarAction
        = application.actionForId(KiriViewApplication::OptionsShowMenubarAction);
    QVERIFY(showMenubarAction != nullptr);
    QVERIFY(showMenubarAction->isCheckable());
    QVERIFY(!showMenubarAction->isChecked());
    QCOMPARE(application.menuPresentation(), KiriViewApplication::HamburgerMenu);

    showMenubarAction->trigger();
    QCOMPARE(application.menuPresentation(), KiriViewApplication::MenuBar);
    QVERIFY(showMenubarAction->isChecked());

    showMenubarAction->trigger();
    QCOMPARE(application.menuPresentation(), KiriViewApplication::HamburgerMenu);
    QVERIFY(!showMenubarAction->isChecked());
}

void TestKiriViewApplication::windowShellApplicationAttachmentIsIdentitySafe()
{
    KiriWindowShell shell;
    KiriViewApplication firstApplication;
    KiriViewApplication secondApplication;

    shell.attachApplication(&firstApplication);
    shell.attachApplication(&firstApplication);
    const int initialRevision = shell.notificationReplayRevision();

    Q_EMIT firstApplication.imageBoundaryReached(QStringLiteral("first boundary"));
    QCOMPARE(shell.notificationReplayRevision(), initialRevision + 1);
    QCOMPARE(shell.notificationMessage(), QStringLiteral("first boundary"));

    shell.attachApplication(&secondApplication);
    const int replacementRevision = shell.notificationReplayRevision();
    Q_EMIT firstApplication.imageBoundaryReached(QStringLiteral("stale boundary"));
    QCOMPARE(shell.notificationReplayRevision(), replacementRevision);

    Q_EMIT secondApplication.imageBoundaryReached(QStringLiteral("second boundary"));
    QCOMPARE(shell.notificationReplayRevision(), replacementRevision + 1);
    QCOMPARE(shell.notificationMessage(), QStringLiteral("second boundary"));

    shell.attachApplication(nullptr);
    const int detachedRevision = shell.notificationReplayRevision();
    Q_EMIT secondApplication.imageBoundaryReached(QStringLiteral("detached boundary"));
    QCOMPARE(shell.notificationReplayRevision(), detachedRevision);
}

void TestKiriViewApplication::windowShellDocumentSessionAttachmentIsIdentitySafe()
{
    KiriWindowShell shell;
    KiriDocumentSession firstSession;
    KiriDocumentSession secondSession;

    shell.attachDocumentSession(&firstSession);
    shell.attachDocumentSession(&firstSession);
    const int initialRevision = shell.notificationReplayRevision();

    Q_EMIT firstSession.fileDeletionFailed(QStringLiteral("first failure"));
    QCOMPARE(shell.notificationReplayRevision(), initialRevision + 1);
    QCOMPARE(shell.notificationMessage(), QStringLiteral("first failure"));

    Q_EMIT firstSession.imageDocument()->containerNavigationBoundaryReached(
        QStringLiteral("first image boundary"));
    QCOMPARE(shell.notificationReplayRevision(), initialRevision + 2);

    shell.attachDocumentSession(&secondSession);
    const int replacementRevision = shell.notificationReplayRevision();
    Q_EMIT firstSession.fileDeletionFailed(QStringLiteral("stale failure"));
    Q_EMIT firstSession.imageDocument()->containerNavigationBoundaryReached(
        QStringLiteral("stale image boundary"));
    QCOMPARE(shell.notificationReplayRevision(), replacementRevision);

    Q_EMIT secondSession.imageDocument()->containerNavigationBoundaryReached(
        QStringLiteral("current image boundary"));
    const int currentRevision = shell.notificationReplayRevision();
    Q_EMIT firstSession.sourceUrlChanged();
    Q_EMIT firstSession.imageDocument()->displayedUrlChanged();
    QCOMPARE(shell.notificationReplayRevision(), currentRevision);
    QVERIFY(shell.notificationActive());
    QCOMPARE(shell.notificationMessage(), QStringLiteral("current image boundary"));

    Q_EMIT secondSession.fileDeletionFailed(QStringLiteral("second failure"));
    QCOMPARE(shell.notificationReplayRevision(), currentRevision + 1);
    QCOMPARE(shell.notificationMessage(), QStringLiteral("second failure"));

    shell.attachDocumentSession(nullptr);
    const int detachedRevision = shell.notificationReplayRevision();
    Q_EMIT secondSession.fileDeletionFailed(QStringLiteral("detached failure"));
    Q_EMIT secondSession.imageDocument()->containerNavigationBoundaryReached(
        QStringLiteral("detached image boundary"));
    QCOMPARE(shell.notificationReplayRevision(), detachedRevision);
}

void TestKiriViewApplication::windowShellRefreshesTitleWhenSessionDetachesOrIsDestroyed()
{
    const QString originalDisplayName = QGuiApplication::applicationDisplayName();
    const auto restoreDisplayName = qScopeGuard([originalDisplayName]() {
        QGuiApplication::setApplicationDisplayName(originalDisplayName);
    });
    Q_UNUSED(restoreDisplayName)

    QGuiApplication::setApplicationDisplayName(QStringLiteral("Attached application"));
    KiriWindowShell shell;
    auto session = std::make_unique<KiriDocumentSession>();
    shell.attachDocumentSession(session.get());
    QCOMPARE(shell.windowTitle(), QStringLiteral("Attached application"));

    QGuiApplication::setApplicationDisplayName(QStringLiteral("Detached application"));
    shell.attachDocumentSession(nullptr);
    QCOMPARE(shell.windowTitle(), QStringLiteral("Detached application"));

    shell.attachDocumentSession(session.get());
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Destroyed application"));
    session.reset();
    QCOMPARE(shell.windowTitle(), QStringLiteral("Destroyed application"));
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    TestKiriViewApplication test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_kiriviewapplication.moc"
