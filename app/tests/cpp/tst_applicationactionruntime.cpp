// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationactionhost.h"
#include "application/applicationactionruntime.h"
#include "application/applicationcommandportsource.h"
#include "application/applicationcommandrouter.h"
#include "application/navigationpresentationprojection.h"
#include "session/documentsessiontypes.h"

#include <KirigamiActionCollection>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QTest>

#include <utility>
#include <vector>

namespace {
namespace Actions = kiriview::ApplicationActions;
using ActionId = kiriview::ApplicationActions::ActionId;

class FakeApplicationActionHost final : public Actions::ApplicationActionHost
{
public:
    FakeApplicationActionHost()
        : collection(&object)
    {
    }

    QObject* actionContext() override { return &object; }
    KirigamiActionCollection* mainActionCollection() override { return &collection; }
    QAction* inheritedAction(const QString& actionName) override
    {
        return collection.action(actionName);
    }
    void readActionSettings() override { }

    QObject object;
    KirigamiActionCollection collection;
};

struct CommandLog
{
    int openDialogCount = 0;
    int previousNavigationCount = 0;
    QStringList escapeCommands;
};

void compareNavigationSlot(
    const Actions::NavigationPresentationSlot& slot, ActionId actionId, ActionId iconActionId)
{
    QCOMPARE(slot.actionId, actionId);
    QCOMPARE(slot.iconActionId, iconActionId);
}

Actions::ApplicationActionStateSnapshot currentImageToolbarSnapshot()
{
    Actions::ApplicationActionStateSnapshot snapshot;
    kiriview::DocumentSessionActionAvailabilityFacts& availability
        = snapshot.documentSession.availability;
    availability.imageReady = true;
    availability.twoPageModeActive = true;
    availability.twoPageModeAvailable = true;
    availability.rightToLeftReadingActive = true;
    availability.rightToLeftReadingAvailable = true;
    availability.fitHeightModeSelected = true;
    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::CurrentAuthoritative;
    snapshot.documentSession.imageFitModeSelection = kiriview::ImageFitModeSelection::FitHeight;
    kiriview::ActiveZoomSnapshot& zoom = snapshot.documentSession.activeZoom;
    zoom.available = true;
    zoom.known = true;
    zoom.percent = 125.0;
    zoom.editable = true;
    zoom.minimumManualPercent = 10;
    zoom.maximumManualPercent = 2'000;
    snapshot.documentSession.imageCollectionControlsVisible = true;
    return snapshot;
}

Actions::ApplicationActionStateSnapshot unavailableOpenedCollectionToolbarSnapshot()
{
    Actions::ApplicationActionStateSnapshot snapshot;
    kiriview::DocumentSessionActionAvailabilityFacts& availability
        = snapshot.documentSession.availability;
    availability.twoPageModeActive = true;
    availability.twoPageModeAvailable = true;
    availability.rightToLeftReadingActive = true;
    availability.rightToLeftReadingAvailable = true;
    snapshot.documentSession.imagePresentationPhase = kiriview::ImagePresentationPhase::Unavailable;
    snapshot.documentSession.imageCollectionControlsVisible = true;
    return snapshot;
}

class FakeCommandPortSource final : public Actions::ApplicationCommandPortSource
{
public:
    Actions::ApplicationCommandRouterShellPorts commandRouterShellPorts() override
    {
        Actions::ApplicationCommandRouterShellPorts ports;
        ports.requestOpenDialog = [this]() { ++log.openDialogCount; };
        return ports;
    }

    Actions::ApplicationCommandRouterSessionPorts commandRouterSessionPorts() override
    {
        Actions::ApplicationCommandRouterSessionPorts ports;
        ports.requestPreviousActiveNavigationWithBoundary
            = [this]() { ++log.previousNavigationCount; };
        return ports;
    }

    Actions::ApplicationCommandRouterToolbarPorts commandRouterToolbarPorts() override
    {
        Actions::ApplicationCommandRouterToolbarPorts ports;
        ports.cancelToolbarTextInputEditing
            = [this]() { log.escapeCommands.push_back(QStringLiteral("cancel-toolbar-edit")); };
        return ports;
    }

    Actions::ApplicationCommandRouterPanelPorts commandRouterPanelPorts() override
    {
        Actions::ApplicationCommandRouterPanelPorts ports;
        ports.closeInfoPanel
            = [this]() { log.escapeCommands.push_back(QStringLiteral("close-info-panel")); };
        return ports;
    }

    Actions::ApplicationCommandRouterWindowPorts commandRouterWindowPorts() override
    {
        Actions::ApplicationCommandRouterWindowPorts ports;
        ports.leaveFullScreen
            = [this]() { log.escapeCommands.push_back(QStringLiteral("leave-fullscreen")); };
        return ports;
    }

    CommandLog log;
};
}

class TestApplicationActionRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void triggeredActionDispatchesThroughRuntimeOwnedRouter();
    void fixedShortcutDispatchesThroughRuntimeOwnedRouter();
    void escapeShortcutDispatchesExactlyOneOwnerCommand();
    void actionStateSnapshotBuildsRuntimePolicyInput();
    void actionStateSnapshotBuildsCommandRouterInput();
    void navigationPresentationProjectionFollowsActionStateSnapshot();
    void retainedImageToolbarKeepsAppearanceUntilCurrentRefinement();
    void unavailableImageToolbarClearsRetainedAppearance();
    void retainedImageToolbarWithoutCurrentHistoryIsUnavailable();
    void manualZoomKeepsSelectedFitPresentation();
    void currentImageToolbarMatchesCanonicalActionState();
    void playableOpenedCollectionVideoToolbarUsesCanonicalPresentation();
    void unsupportedOpenedCollectionVideoToolbarUsesCanonicalPresentation();
};

void TestApplicationActionRuntime::triggeredActionDispatchesThroughRuntimeOwnedRouter()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    FakeCommandPortSource portSource;

    runtime.setCommandPortSource(&portSource);
    runtime.handleActionTriggered(ActionId::FileOpenAction);

    QCOMPARE(portSource.log.openDialogCount, 1);
}

void TestApplicationActionRuntime::fixedShortcutDispatchesThroughRuntimeOwnedRouter()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    FakeCommandPortSource portSource;
    Actions::ApplicationActionStateSnapshot snapshot;
    snapshot.documentSession.videoMode = true;

    runtime.setCommandPortSource(&portSource);
    runtime.setActionStateSnapshot(snapshot);

    QVERIFY(runtime.executeHorizontalArrowShortcut(true));

    QCOMPARE(portSource.log.previousNavigationCount, 1);
}

void TestApplicationActionRuntime::escapeShortcutDispatchesExactlyOneOwnerCommand()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    FakeCommandPortSource portSource;
    runtime.setCommandPortSource(&portSource);

    const auto expectSingleCommand
        = [&runtime, &portSource](
              Actions::FixedShortcutDispatchKind decision, const QString& expectedCommand) {
              portSource.log.escapeCommands.clear();

              QVERIFY(runtime.executeEscapeShortcut(decision));

              QCOMPARE(portSource.log.escapeCommands, QStringList { expectedCommand });
          };

    expectSingleCommand(Actions::FixedShortcutDispatchKind::CancelToolbarTextInput,
        QStringLiteral("cancel-toolbar-edit"));
    expectSingleCommand(
        Actions::FixedShortcutDispatchKind::CloseInfoPanel, QStringLiteral("close-info-panel"));
    expectSingleCommand(
        Actions::FixedShortcutDispatchKind::ExitFullscreen, QStringLiteral("leave-fullscreen"));

    portSource.log.escapeCommands.clear();
    QVERIFY(!runtime.executeEscapeShortcut(Actions::FixedShortcutDispatchKind::None));
    QVERIFY(portSource.log.escapeCommands.isEmpty());
}

void TestApplicationActionRuntime::actionStateSnapshotBuildsRuntimePolicyInput()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionStateSnapshot snapshot;
    snapshot.uiGateRevision = 7;
    snapshot.documentSession.availability = kiriview::DocumentSessionActionAvailabilityFacts {
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
    };
    snapshot.documentSession.activeNavigation
        = kiriview::ActiveNavigationSnapshot { true, true, true, true, true, false, false, 2, 3 };
    snapshot.documentSession.displayedMediaOpenWithAvailable = true;
    snapshot.documentSession.displayedFileDeletionAvailable = true;
    snapshot.documentSession.imagePannable = true;
    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::CurrentAuthoritative;
    snapshot.uiGates.applicationMenuShortcutEnabled = true;
    snapshot.uiGates.showMenubarActionEnabled = false;

    runtime.setActionStateSnapshot(snapshot);

    QCOMPARE(runtime.actionStateRevision(), 1);
    QVERIFY(runtime.actionPlacementEnabled(ActionId::GoPreviousImageAction));
    QVERIFY(runtime.actionPlacementEnabled(ActionId::ViewToggleRightToLeftReadingAction));
    QCOMPARE(runtime.actionMenuText(ActionId::ViewFitAction), QStringLiteral("Fit to &Window"));
    QVERIFY(runtime.rightToLeftReadingActive());
}

void TestApplicationActionRuntime::actionStateSnapshotBuildsCommandRouterInput()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionStateSnapshot snapshot;
    snapshot.documentSession.availability.rightToLeftReadingActive = true;
    snapshot.documentSession.availability.rightToLeftReadingAvailable = true;
    snapshot.documentSession.imagePannable = true;
    snapshot.documentSession.videoMode = true;
    snapshot.documentSession.activeNavigationBoundaryScope
        = kiriview::ActiveNavigationBoundaryScope::ImageDocumentPage;
    snapshot.documentSession.activeNavigation.atKnownFirst = true;
    snapshot.documentSession.activeNavigation.canOpenPrevious = true;

    runtime.setActionStateSnapshot(snapshot);

    const Actions::ApplicationCommandRouterInput input = runtime.commandRouterInput();
    QVERIFY(input.imagePannable);
    QVERIFY(input.rightToLeftReadingActive);
    QVERIFY(input.videoMode);
    QVERIFY(input.imageDocumentPageNavigationActive);
    QVERIFY(input.atKnownFirstActiveNavigation);
    QVERIFY(input.canOpenPreviousActiveNavigation);
}

void TestApplicationActionRuntime::navigationPresentationProjectionFollowsActionStateSnapshot()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);

    Actions::NavigationPresentationProjection projection
        = runtime.navigationPresentationProjection();
    compareNavigationSlot(projection.leadingImageAction, ActionId::GoPreviousImageAction,
        ActionId::GoPreviousImageAction);
    compareNavigationSlot(projection.trailingArchiveMenuAction, ActionId::GoNextArchiveAction,
        ActionId::GoNextArchiveAction);

    Actions::ApplicationActionStateSnapshot snapshot;
    snapshot.documentSession.availability.imageReady = true;
    snapshot.documentSession.availability.rightToLeftReadingActive = true;
    snapshot.documentSession.availability.rightToLeftReadingAvailable = true;

    runtime.setActionStateSnapshot(snapshot);

    projection = runtime.navigationPresentationProjection();
    compareNavigationSlot(projection.leadingImageAction, ActionId::GoNextImageAction,
        ActionId::GoPreviousImageAction);
    compareNavigationSlot(projection.trailingArchiveMenuAction, ActionId::GoPreviousArchiveAction,
        ActionId::GoNextArchiveAction);
}

void TestApplicationActionRuntime::retainedImageToolbarKeepsAppearanceUntilCurrentRefinement()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime* observedRuntime = nullptr;
    std::vector<Actions::ImageToolbarPresentationSnapshot> publishedPresentations;
    Actions::ApplicationActionRuntime::Callbacks callbacks;
    callbacks.actionStateChanged = [&observedRuntime, &publishedPresentations]() {
        QVERIFY(observedRuntime != nullptr);
        publishedPresentations.push_back(observedRuntime->imageToolbarPresentationSnapshot());
    };
    Actions::ApplicationActionRuntime runtime(host, std::move(callbacks));
    observedRuntime = &runtime;
    runtime.setupActions();
    Actions::ApplicationActionStateSnapshot snapshot = currentImageToolbarSnapshot();

    runtime.setActionStateSnapshot(snapshot);

    QCOMPARE(publishedPresentations.size(), std::size_t(1));
    const Actions::ImageToolbarPresentationSnapshot current = publishedPresentations.back();
    QCOMPARE(current.phase, kiriview::ImagePresentationPhase::CurrentAuthoritative);
    QVERIFY(current.collectionControlsVisible);
    QVERIFY(current.rightToLeftReading.appearanceEnabled);
    QVERIFY(current.rightToLeftReading.appearanceChecked);
    QVERIFY(current.rightToLeftReading.interactionEnabled);
    QVERIFY(current.twoPageMode.appearanceEnabled);
    QVERIFY(current.twoPageMode.appearanceChecked);
    QVERIFY(current.twoPageMode.interactionEnabled);
    QVERIFY(current.fitMode.appearanceEnabled);
    QVERIFY(current.fitMode.appearanceChecked);
    QVERIFY(current.fitMode.interactionEnabled);
    QCOMPARE(current.presentedFitActionId, ActionId::ViewFitHeightAction);
    QVERIFY(current.zoom.appearanceEnabled);
    QVERIFY(current.zoom.interactionEnabled);
    QVERIFY(current.zoom.available);
    QVERIFY(current.zoom.known);
    QVERIFY(current.zoom.editable);
    QCOMPARE(current.zoom.percent, 125.0);
    QCOMPARE(current.zoom.minimumManualPercent, 10);
    QCOMPARE(current.zoom.maximumManualPercent, 2'000);

    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
    snapshot.documentSession.imageCollectionControlsVisible = false;
    runtime.setActionStateSnapshot(snapshot);

    QCOMPARE(publishedPresentations.size(), std::size_t(2));
    const Actions::ImageToolbarPresentationSnapshot retained = publishedPresentations.back();
    QCOMPARE(retained.phase, kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative);
    QVERIFY(!retained.collectionControlsVisible);
    QCOMPARE(retained.rightToLeftReading.appearanceEnabled,
        current.rightToLeftReading.appearanceEnabled);
    QCOMPARE(retained.rightToLeftReading.appearanceChecked,
        current.rightToLeftReading.appearanceChecked);
    QVERIFY(!retained.rightToLeftReading.interactionEnabled);
    QCOMPARE(retained.twoPageMode.appearanceEnabled, current.twoPageMode.appearanceEnabled);
    QCOMPARE(retained.twoPageMode.appearanceChecked, current.twoPageMode.appearanceChecked);
    QVERIFY(!retained.twoPageMode.interactionEnabled);
    QCOMPARE(retained.fitMode.appearanceEnabled, current.fitMode.appearanceEnabled);
    QCOMPARE(retained.fitMode.appearanceChecked, current.fitMode.appearanceChecked);
    QVERIFY(!retained.fitMode.interactionEnabled);
    QCOMPARE(retained.presentedFitActionId, current.presentedFitActionId);
    QCOMPARE(retained.zoom.appearanceEnabled, current.zoom.appearanceEnabled);
    QVERIFY(!retained.zoom.interactionEnabled);
    QCOMPARE(retained.zoom.available, current.zoom.available);
    QCOMPARE(retained.zoom.known, current.zoom.known);
    QCOMPARE(retained.zoom.editable, current.zoom.editable);
    QCOMPARE(retained.zoom.percent, current.zoom.percent);
    QCOMPARE(retained.zoom.minimumManualPercent, current.zoom.minimumManualPercent);
    QCOMPARE(retained.zoom.maximumManualPercent, current.zoom.maximumManualPercent);
    const QList<ActionId> collectionActions {
        ActionId::ViewToggleRightToLeftReadingAction,
        ActionId::ViewToggleTwoPageModeAction,
    };
    for (ActionId actionId : collectionActions) {
        QAction* action = runtime.actionForId(actionId);
        QVERIFY(action != nullptr);
        QVERIFY(action->isEnabled());
    }
    const QList<ActionId> readinessDependentActions {
        ActionId::ViewFitHeightAction,
        ActionId::ViewZoomInAction,
    };
    for (ActionId actionId : readinessDependentActions) {
        QAction* action = runtime.actionForId(actionId);
        QVERIFY(action != nullptr);
        QVERIFY(!action->isEnabled());
    }

    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::CurrentAuthoritative;
    kiriview::DocumentSessionActionAvailabilityFacts& refinedAvailability
        = snapshot.documentSession.availability;
    refinedAvailability.imageReady = true;
    refinedAvailability.twoPageModeActive = false;
    refinedAvailability.twoPageModeAvailable = true;
    refinedAvailability.rightToLeftReadingActive = false;
    refinedAvailability.rightToLeftReadingAvailable = true;
    refinedAvailability.fitWidthModeSelected = true;
    snapshot.documentSession.imageFitModeSelection = kiriview::ImageFitModeSelection::FitWidth;
    kiriview::ActiveZoomSnapshot& refinedZoom = snapshot.documentSession.activeZoom;
    refinedZoom.available = true;
    refinedZoom.known = true;
    refinedZoom.percent = 150.0;
    refinedZoom.editable = true;
    refinedZoom.minimumManualPercent = 10;
    refinedZoom.maximumManualPercent = 3'000;
    runtime.setActionStateSnapshot(snapshot);

    QCOMPARE(publishedPresentations.size(), std::size_t(3));
    const Actions::ImageToolbarPresentationSnapshot refined = publishedPresentations.back();
    QCOMPARE(refined.phase, kiriview::ImagePresentationPhase::CurrentAuthoritative);
    QVERIFY(!refined.rightToLeftReading.appearanceChecked);
    QVERIFY(!refined.twoPageMode.appearanceChecked);
    QCOMPARE(refined.presentedFitActionId, ActionId::ViewFitWidthAction);
    QVERIFY(refined.fitMode.appearanceChecked);
    QCOMPARE(refined.zoom.percent, 150.0);
    QCOMPARE(refined.zoom.minimumManualPercent, 10);
    QCOMPARE(refined.zoom.maximumManualPercent, 3'000);
}

void TestApplicationActionRuntime::unavailableImageToolbarClearsRetainedAppearance()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionStateSnapshot snapshot = currentImageToolbarSnapshot();
    runtime.setActionStateSnapshot(snapshot);

    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
    snapshot.documentSession.availability = {};
    snapshot.documentSession.activeZoom = {};
    runtime.setActionStateSnapshot(snapshot);
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().phase,
        kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative);

    snapshot.documentSession.imagePresentationPhase = kiriview::ImagePresentationPhase::Unavailable;
    runtime.setActionStateSnapshot(snapshot);

    const Actions::ImageToolbarPresentationSnapshot unavailable
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(unavailable.phase, kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(!unavailable.rightToLeftReading.appearanceEnabled);
    QVERIFY(!unavailable.rightToLeftReading.appearanceChecked);
    QVERIFY(!unavailable.rightToLeftReading.interactionEnabled);
    QVERIFY(!unavailable.twoPageMode.appearanceEnabled);
    QVERIFY(!unavailable.twoPageMode.appearanceChecked);
    QVERIFY(!unavailable.twoPageMode.interactionEnabled);
    QVERIFY(!unavailable.fitMode.appearanceEnabled);
    QVERIFY(!unavailable.fitMode.appearanceChecked);
    QVERIFY(!unavailable.fitMode.interactionEnabled);
    QCOMPARE(unavailable.presentedFitActionId, ActionId::ViewFitAction);
    QVERIFY(!unavailable.zoom.appearanceEnabled);
    QVERIFY(!unavailable.zoom.interactionEnabled);
    QVERIFY(!unavailable.zoom.available);
    QVERIFY(!unavailable.zoom.known);
    QVERIFY(!unavailable.zoom.editable);
    QCOMPARE(unavailable.zoom.percent, 0.0);
    QCOMPARE(unavailable.zoom.minimumManualPercent, 0);
    QCOMPARE(unavailable.zoom.maximumManualPercent, 0);

    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
    runtime.setActionStateSnapshot(snapshot);
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().phase,
        kiriview::ImagePresentationPhase::Unavailable);
}

void TestApplicationActionRuntime::retainedImageToolbarWithoutCurrentHistoryIsUnavailable()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionStateSnapshot snapshot;
    snapshot.documentSession.imagePresentationPhase
        = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
    snapshot.documentSession.imageCollectionControlsVisible = true;

    runtime.setActionStateSnapshot(snapshot);

    const Actions::ImageToolbarPresentationSnapshot presentation
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(presentation.phase, kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(presentation.collectionControlsVisible);
    QVERIFY(!presentation.rightToLeftReading.appearanceEnabled);
    QVERIFY(!presentation.rightToLeftReading.interactionEnabled);
    QVERIFY(!presentation.twoPageMode.appearanceEnabled);
    QVERIFY(!presentation.twoPageMode.interactionEnabled);
    QVERIFY(!presentation.fitMode.appearanceEnabled);
    QVERIFY(!presentation.fitMode.interactionEnabled);
    QVERIFY(!presentation.zoom.appearanceEnabled);
    QVERIFY(!presentation.zoom.interactionEnabled);
}

void TestApplicationActionRuntime::manualZoomKeepsSelectedFitPresentation()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionStateSnapshot snapshot = currentImageToolbarSnapshot();
    snapshot.documentSession.availability.fitHeightModeSelected = false;

    runtime.setActionStateSnapshot(snapshot);

    const Actions::ImageToolbarPresentationSnapshot presentation
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(presentation.presentedFitActionId, ActionId::ViewFitHeightAction);
    QVERIFY(presentation.fitMode.appearanceEnabled);
    QVERIFY(!presentation.fitMode.appearanceChecked);
    QVERIFY(presentation.fitMode.interactionEnabled);
}

void TestApplicationActionRuntime::currentImageToolbarMatchesCanonicalActionState()
{
    FakeApplicationActionHost host;
    int actionStateCommitCount = 0;
    Actions::ApplicationActionRuntime::Callbacks callbacks;
    callbacks.actionStateChanged = [&actionStateCommitCount]() { ++actionStateCommitCount; };
    Actions::ApplicationActionRuntime runtime(host, std::move(callbacks));
    runtime.setupActions();
    Actions::ApplicationActionStateSnapshot snapshot = currentImageToolbarSnapshot();

    runtime.setActionStateSnapshot(snapshot);

    const auto compareAction = [&runtime](ActionId actionId) {
        QAction* action = runtime.actionForId(actionId);
        QVERIFY(action != nullptr);
        const Actions::ImageToolbarActionPresentation presentation
            = runtime.imageToolbarActionPresentation(actionId);
        QCOMPARE(presentation.appearanceEnabled, action->isEnabled());
        QCOMPARE(presentation.appearanceChecked, action->isChecked());
        QCOMPARE(presentation.interactionEnabled, action->isEnabled());
    };
    compareAction(ActionId::ViewToggleRightToLeftReadingAction);
    compareAction(ActionId::ViewToggleTwoPageModeAction);
    compareAction(ActionId::ViewFitHeightAction);
    QAction* zoomInAction = runtime.actionForId(ActionId::ViewZoomInAction);
    QVERIFY(zoomInAction != nullptr);
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().zoom.appearanceEnabled,
        zoomInAction->isEnabled());
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().zoom.interactionEnabled,
        zoomInAction->isEnabled());
    QCOMPARE(runtime.actionStateRevision(), 1);
    QCOMPARE(actionStateCommitCount, 1);

    snapshot.uiGates.helpDialogOpen = true;
    runtime.setActionStateSnapshot(snapshot);

    QCOMPARE(runtime.imageToolbarPresentationSnapshot().phase,
        kiriview::ImagePresentationPhase::CurrentAuthoritative);
    compareAction(ActionId::ViewToggleRightToLeftReadingAction);
    compareAction(ActionId::ViewToggleTwoPageModeAction);
    compareAction(ActionId::ViewFitHeightAction);
    QVERIFY(!runtime.imageToolbarPresentationSnapshot().zoom.appearanceEnabled);
    QVERIFY(!runtime.imageToolbarPresentationSnapshot().zoom.interactionEnabled);
    QVERIFY(!zoomInAction->isEnabled());
    QCOMPARE(runtime.actionStateRevision(), 2);
    QCOMPARE(actionStateCommitCount, 2);
}

void TestApplicationActionRuntime::playableOpenedCollectionVideoToolbarUsesCanonicalPresentation()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    runtime.setupActions();
    Actions::ApplicationActionStateSnapshot snapshot = unavailableOpenedCollectionToolbarSnapshot();
    snapshot.documentSession.videoMode = true;
    snapshot.documentSession.availability.containerNavigationAvailable = true;
    snapshot.documentSession.activeZoom = kiriview::ActiveZoomSnapshot {
        true,
        true,
        71.0,
        false,
    };

    runtime.setActionStateSnapshot(snapshot);

    const Actions::ImageToolbarPresentationSnapshot presentation
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(presentation.phase, kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(presentation.collectionControlsVisible);
    QVERIFY(presentation.rightToLeftReading.appearanceEnabled);
    QVERIFY(presentation.rightToLeftReading.appearanceChecked);
    QVERIFY(presentation.rightToLeftReading.interactionEnabled);
    QVERIFY(presentation.twoPageMode.appearanceEnabled);
    QVERIFY(presentation.twoPageMode.appearanceChecked);
    QVERIFY(presentation.twoPageMode.interactionEnabled);
    QVERIFY(!presentation.fitMode.appearanceEnabled);
    QVERIFY(!presentation.fitMode.interactionEnabled);
    QVERIFY(presentation.zoom.appearanceEnabled);
    QVERIFY(!presentation.zoom.interactionEnabled);
    QVERIFY(presentation.zoom.available);
    QVERIFY(presentation.zoom.known);
    QVERIFY(!presentation.zoom.editable);
    QCOMPARE(presentation.zoom.percent, 71.0);
    QCOMPARE(presentation.zoom.minimumManualPercent, 0);
    QCOMPARE(presentation.zoom.maximumManualPercent, 0);

    QAction* rightToLeftAction = runtime.actionForId(ActionId::ViewToggleRightToLeftReadingAction);
    QAction* twoPageAction = runtime.actionForId(ActionId::ViewToggleTwoPageModeAction);
    QAction* previousArchiveAction = runtime.actionForId(ActionId::GoPreviousArchiveAction);
    QVERIFY(rightToLeftAction != nullptr);
    QVERIFY(twoPageAction != nullptr);
    QVERIFY(previousArchiveAction != nullptr);
    QVERIFY(rightToLeftAction->isEnabled());
    QVERIFY(rightToLeftAction->isChecked());
    QVERIFY(twoPageAction->isEnabled());
    QVERIFY(twoPageAction->isChecked());
    QVERIFY(previousArchiveAction->isEnabled());
}

void TestApplicationActionRuntime::
    unsupportedOpenedCollectionVideoToolbarUsesCanonicalPresentation()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    runtime.setupActions();
    const Actions::ApplicationActionStateSnapshot snapshot
        = unavailableOpenedCollectionToolbarSnapshot();

    runtime.setActionStateSnapshot(snapshot);

    const Actions::ImageToolbarPresentationSnapshot presentation
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(presentation.phase, kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(presentation.collectionControlsVisible);
    QVERIFY(presentation.rightToLeftReading.appearanceEnabled);
    QVERIFY(presentation.rightToLeftReading.appearanceChecked);
    QVERIFY(presentation.rightToLeftReading.interactionEnabled);
    QVERIFY(presentation.twoPageMode.appearanceEnabled);
    QVERIFY(presentation.twoPageMode.appearanceChecked);
    QVERIFY(presentation.twoPageMode.interactionEnabled);
    QVERIFY(!presentation.fitMode.appearanceEnabled);
    QVERIFY(!presentation.fitMode.interactionEnabled);
    QVERIFY(!presentation.zoom.appearanceEnabled);
    QVERIFY(!presentation.zoom.interactionEnabled);
    QVERIFY(!presentation.zoom.available);
    QVERIFY(!presentation.zoom.known);
    QVERIFY(!presentation.zoom.editable);
    QCOMPARE(presentation.zoom.percent, 0.0);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    TestApplicationActionRuntime test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_applicationactionruntime.moc"
