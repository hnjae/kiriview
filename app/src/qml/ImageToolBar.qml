// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQml.Models
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami

Controls.ToolBar {
    id: root

    required property KiriImageDocument imageDocument
    required property bool imageReady
    required property int minimumManualZoomPercent
    required property int maximumManualZoomPercent
    required property real zoomStepFactor
    required property var actions
    required property var navigationPresentationProvider
    property bool compact: false
    property bool floating: false
    property bool transientOverlay: false
    property var applicationMenuActions: []
    property bool showApplicationMenuActions: false
    property bool videoMode: false
    property bool replacementGraceActive: false
    property bool zoomPercentAvailable: imageReady
    property bool zoomPercentKnown: imageReady
    property real zoomPercent: imageDocument.zoomPercent
    property bool zoomEditable: !videoMode && imageReady
    property bool activeNavigationAvailable: false
    property int activeNavigationCount: 0
    property int activeNavigationCurrentNumber: 0
    property bool activeNavigationEditable: false
    property bool activeNavigationKnown: false
    property var openActiveNavigationAtNumber: function (number) {}
    property bool collectionControlsVisible: false
    readonly property Item applicationMenuButtonAnchor: applicationMenuCoordinator.buttonAnchor
    readonly property int controlSpacing: compact ? Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2)) : Kirigami.Units.smallSpacing
    readonly property int edgeMargin: controlSpacing
    readonly property int fitModeSelection: imageDocument?.fitModeSelection ?? KiriImageDocument.Fit
    readonly property bool fitMenuButtonTextVisible: width >= Kirigami.Units.gridUnit * 40
    readonly property bool interactionActive: textInputCoordinator.active || applicationMenuCoordinator.open
    readonly property bool readyImageControlPresentationRetained: retainedPresentation.presentationRetained
    readonly property int presentedFitModeSelection: retainedPresentation.presentedFitModeSelection
    readonly property bool presentedImageReady: retainedPresentation.presentedImageReady
    readonly property bool presentedZoomEditable: retainedPresentation.presentedZoomEditable
    readonly property bool presentedZoomPercentAvailable: retainedPresentation.presentedZoomPercentAvailable
    readonly property bool presentedZoomPercentKnown: retainedPresentation.presentedZoomPercentKnown
    readonly property real presentedZoomPercent: retainedPresentation.presentedZoomPercent
    readonly property int toolbarVerticalPadding: controlSpacing
    readonly property var imageToolbarControls: (root.collectionControlsVisible ? [rightToLeftToolbarAction, twoPageToolbarAction] : []).concat([fitMenuAction, zoomLevelAction])
    readonly property var toolbarControls: imageToolbarControls
    readonly property var toolbarActions: showApplicationMenuActions ? toolbarControls.concat([applicationMenuAction]) : toolbarControls

    signal textInputFocusReturnRequested

    function cancelTextInputEditing(returnViewerFocus) {
        return textInputCoordinator.cancel(returnViewerFocus);
    }

    function commitTextInputEditing(returnViewerFocus) {
        return textInputCoordinator.commit(returnViewerFocus);
    }

    function applicationMenuOpen() {
        return applicationMenuCoordinator.open;
    }

    function applicationMenuButtonUsable(button) {
        return applicationMenuCoordinator.buttonUsable(button);
    }

    function updateApplicationMenuButtonAnchor(button) {
        applicationMenuCoordinator.updateButtonAnchor(button);
    }

    function popupApplicationMenu() {
        return applicationMenuCoordinator.popupMenu();
    }

    function openApplicationMenu() {
        return applicationMenuCoordinator.openMenu();
    }

    function toggleApplicationMenu() {
        return applicationMenuCoordinator.toggleMenu();
    }

    function resetPageNumberText() {
        textInputCoordinator.resetPageNumber();
    }

    function textInputFocused() {
        return textInputCoordinator.active;
    }

    function fitModeIsSelectable(zoomMode) {
        return zoomMode === KiriImageDocument.Fit || zoomMode === KiriImageDocument.FitHeight || zoomMode === KiriImageDocument.FitWidth;
    }

    function fitModeAction(zoomMode) {
        if (zoomMode === KiriImageDocument.FitHeight) {
            return root.actions.fitHeightAction;
        }
        if (zoomMode === KiriImageDocument.FitWidth) {
            return root.actions.fitWidthAction;
        }
        return root.actions.fitAction;
    }

    function fitModeIconName(zoomMode) {
        if (zoomMode === KiriImageDocument.FitHeight) {
            return "zoom-fit-height";
        }
        if (zoomMode === KiriImageDocument.FitWidth) {
            return "zoom-fit-width";
        }
        return "zoom-fit-best-symbolic";
    }

    function fitModeText(zoomMode) {
        if (zoomMode === KiriImageDocument.FitHeight) {
            return KI18n.i18nc("@action:button", "Fit Height");
        }
        if (zoomMode === KiriImageDocument.FitWidth) {
            return KI18n.i18nc("@action:button", "Fit Width");
        }
        return KI18n.i18nc("@action:button", "Fit to Window");
    }

    function triggerFitMode(zoomMode) {
        if (!fitModeIsSelectable(zoomMode)) {
            return;
        }

        const action = fitModeAction(zoomMode);
        if (action !== null && action !== undefined && action.enabled) {
            action.trigger();
        }
    }

    leftPadding: edgeMargin
    rightPadding: edgeMargin
    topPadding: toolbarVerticalPadding
    bottomPadding: toolbarVerticalPadding

    Component.onCompleted: {
        if (floating) {
            background = floatingBackgroundComponent.createObject(root);
        }
    }

    ImageToolBarRetainedPresentation {
        id: retainedPresentation

        fitEnabled: root.fitModeAction(root.fitModeSelection)?.enabled ?? false
        fitModeSelection: root.fitModeSelection
        imageReady: root.imageReady
        replacementGraceActive: root.replacementGraceActive
        rightToLeftSourceAction: root.actions.rightToLeftReadingAction
        twoPageSourceAction: root.actions.twoPageModeAction
        videoMode: root.videoMode
        zoomEditable: root.zoomEditable
        zoomPercent: root.zoomPercent
        zoomPercentAvailable: root.zoomPercentAvailable
        zoomPercentKnown: root.zoomPercentKnown
    }

    ImageToolBarTextInputCoordinator {
        id: textInputCoordinator

        onFocusReturnRequested: root.textInputFocusReturnRequested()
    }

    ImageToolBarMenuCoordinator {
        id: applicationMenuCoordinator

        actionSurface: actionToolBar
        anchorItem: applicationMenuPopupAnchor
        hostToolbar: root
        menuActions: root.applicationMenuActions
        menuEnabled: root.showApplicationMenuActions
        menuPopup: applicationMenuPopup
    }

    HoverHandler {
        id: toolbarHoverHandler

        enabled: root.floating || root.transientOverlay
    }

    Component {
        id: floatingBackgroundComponent

        Kirigami.ShadowedRectangle {
            color: Kirigami.Theme.backgroundColor
            opacity: 0.84
            radius: Kirigami.Units.cornerRadius

            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
            border.width: 1

            shadow.color: Qt.rgba(0, 0, 0, 0.35)
            shadow.size: Kirigami.Units.smallSpacing
            shadow.xOffset: 0
            shadow.yOffset: Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2))
        }
    }

    readonly property var rightToLeftToolbarAction: retainedPresentation.rightToLeftAction
    readonly property var twoPageToolbarAction: retainedPresentation.twoPageAction

    readonly property Kirigami.Action zoomLevelAction: Kirigami.Action {
        displayComponent: ImageZoomControls {
            id: zoomControls

            compact: root.compact
            imageDocument: root.imageDocument
            imageReady: root.presentedImageReady
            interactionEnabled: root.zoomEditable
            maximumManualZoomPercent: root.maximumManualZoomPercent
            minimumManualZoomPercent: root.minimumManualZoomPercent
            readOnlyDisplayMode: root.videoMode
            readOnlyPercent: Math.round(root.presentedZoomPercent)
            readOnlyPercentKnown: root.presentedZoomPercentKnown
            presentationEnabled: root.presentedZoomEditable
            zoomEditable: root.zoomEditable
            zoomPercent: root.presentedZoomPercent
            zoomPercentAvailable: root.presentedZoomPercentAvailable
            zoomPercentKnown: root.presentedZoomPercentKnown
            zoomStepFactor: root.zoomStepFactor

            Component.onDestruction: {
                if (textInputActive) {
                    textInputCoordinator.zoomFocused = false;
                }
            }
            onTextInputActiveChanged: textInputCoordinator.zoomFocused = textInputActive

            onEditingCompleted: function (returnViewerFocus) {
                if (returnViewerFocus) {
                    textInputCoordinator.focusReturnRequested();
                }
            }

            Connections {
                target: textInputCoordinator

                function onCancelRequested(returnViewerFocus) {
                    zoomControls.cancelEditing(returnViewerFocus);
                }

                function onCommitRequested(returnViewerFocus) {
                    zoomControls.commitEditing(returnViewerFocus);
                }
            }
        }
        displayHint: Kirigami.DisplayHint.KeepVisible
        enabled: root.readyImageControlPresentationRetained ? retainedPresentation.retainedZoomActionEnabled : (!root.videoMode && root.imageReady)
        icon.name: "zoom-original-symbolic"
        text: KI18n.i18nc("@action", "Zoom")
        tooltip: root.videoMode ? (root.zoomPercentKnown ? KI18n.i18nc("@info:tooltip", "Fitted video zoom") : KI18n.i18nc("@info:tooltip", "Video zoom unavailable")) : text
    }

    readonly property Kirigami.Action fitMenuAction: Kirigami.Action {
        displayComponent: FitModeMenuButton {
            action: root.fitMenuAction
            fitAction: root.actions.fitAction
            fitHeightAction: root.actions.fitHeightAction
            fitHeightMode: KiriImageDocument.FitHeight
            fitMode: KiriImageDocument.Fit
            fitWidthAction: root.actions.fitWidthAction
            fitWidthMode: KiriImageDocument.FitWidth
            fitModeSelection: root.presentedFitModeSelection
            interactionEnabled: root.fitModeAction(root.fitModeSelection).enabled
            textVisible: root.fitMenuButtonTextVisible

            onFitModeTriggered: function (zoomMode) {
                root.triggerFitMode(zoomMode);
            }
        }
        displayHint: Kirigami.DisplayHint.KeepVisible
        enabled: root.readyImageControlPresentationRetained ? retainedPresentation.retainedFitEnabled : root.fitModeAction(root.fitModeSelection).enabled
        icon.name: root.fitModeIconName(root.presentedFitModeSelection)
        text: root.fitModeText(root.presentedFitModeSelection)
        tooltip: text

        onTriggered: root.triggerFitMode(root.fitModeSelection)
    }

    readonly property Kirigami.Action applicationMenuAction: Kirigami.Action {
        displayComponent: Controls.ToolButton {
            id: applicationMenuButton

            objectName: "toolbarApplicationMenuButton"

            Accessible.name: root.applicationMenuAction.text
            Accessible.role: Accessible.ButtonMenu
            Accessible.ignored: !visible

            property bool skipNextClick: false

            display: Controls.AbstractButton.IconOnly
            enabled: root.applicationMenuAction.enabled
            icon.name: root.applicationMenuAction.icon.name
            text: root.applicationMenuAction.text

            Controls.ToolTip.text: root.applicationMenuAction.tooltip
            Controls.ToolTip.visible: hovered && Controls.ToolTip.text.length > 0 && !applicationMenuPopup.visible && !pressed && !Kirigami.Settings.hasTransientTouchInput

            Component.onCompleted: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            Component.onDestruction: {
                if (root.applicationMenuButtonAnchor === applicationMenuButton) {
                    applicationMenuCoordinator.buttonAnchor = null;
                }
            }
            onHeightChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            onParentChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            onVisibleChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            onWidthChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            onXChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)
            onYChanged: root.updateApplicationMenuButtonAnchor(applicationMenuButton)

            onClicked: {
                if (skipNextClick) {
                    skipNextClick = false;
                    return;
                }
                if (Date.now() - applicationMenuCoordinator.closedTimestamp < 250) {
                    return;
                }

                root.toggleApplicationMenu();
            }
            onPressed: {
                if (root.applicationMenuOpen()) {
                    skipNextClick = true;
                    applicationMenuPopup.dismiss();
                }
            }
        }
        displayHint: Kirigami.DisplayHint.KeepVisible
        enabled: root.applicationMenuActions.length > 0
        icon.name: "open-menu-symbolic"
        text: KI18n.i18nc("@action", "Application Menu")
        tooltip: KI18n.i18nc("@info:tooltip", "Open menu") + " (F10)"

        onTriggered: root.toggleApplicationMenu()
    }

    Item {
        id: applicationMenuPopupAnchor

        enabled: false
        height: 1
        width: 1
    }

    Controls.Menu {
        id: applicationMenuPopup

        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        popupType: Controls.Popup.Window

        onAboutToHide: applicationMenuCoordinator.recordClosed()

        MenuAccessKeyRouter {
            enabled: root.showApplicationMenuActions
            menu: applicationMenuPopup
        }

        Instantiator {
            model: root.applicationMenuActions

            delegate: DelegateChooser {
                role: "separator"

                DelegateChoice {
                    roleValue: true

                    Controls.MenuSeparator {}
                }

                DelegateChoice {
                    roleValue: false

                    MenuActionItem {
                        required property var modelData

                        action: modelData
                    }
                }
            }

            onObjectAdded: (index, object) => applicationMenuPopup.insertItem(index, object)
            onObjectRemoved: (index, object) => applicationMenuPopup.removeItem(object)
        }
    }

    contentItem: RowLayout {
        spacing: root.controlSpacing

        ImageDocumentPageNavigation {
            id: pageNavigation

            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

            actions: root.actions
            activeNavigationAvailable: root.activeNavigationAvailable
            activeNavigationCount: root.activeNavigationCount
            activeNavigationCurrentNumber: root.activeNavigationCurrentNumber
            activeNavigationEditable: root.activeNavigationEditable
            activeNavigationKnown: root.activeNavigationKnown
            compact: root.compact
            navigationPresentationProvider: root.navigationPresentationProvider
            openActiveNavigationAtNumber: root.openActiveNavigationAtNumber

            Component.onDestruction: {
                if (textInputActive) {
                    textInputCoordinator.pageNavigationFocused = false;
                }
            }
            onTextInputActiveChanged: textInputCoordinator.pageNavigationFocused = textInputActive

            onEditingCompleted: function (returnViewerFocus) {
                if (returnViewerFocus) {
                    textInputCoordinator.focusReturnRequested();
                }
            }

            Connections {
                target: textInputCoordinator

                function onPageNumberResetRequested() {
                    pageNavigation.resetPageNumberText();
                }

                function onCancelRequested(returnViewerFocus) {
                    pageNavigation.cancelEditing(returnViewerFocus);
                }

                function onCommitRequested(returnViewerFocus) {
                    pageNavigation.commitEditing(returnViewerFocus);
                }
            }
        }

        Kirigami.ActionToolBar {
            id: actionToolBar

            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 2

            actions: root.toolbarActions
            alignment: Qt.AlignRight
            display: Controls.AbstractButton.TextBesideIcon
        }
    }
}
