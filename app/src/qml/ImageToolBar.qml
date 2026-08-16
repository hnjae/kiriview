// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Templates as Templates
import QtQml.Models
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

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
    property bool zoomPercentAvailable: imageReady
    property bool zoomPercentKnown: imageReady
    property real zoomPercent: imageDocument.zoomPercent
    property bool zoomEditable: imageReady
    property bool activeNavigationAvailable: false
    property int activeNavigationCount: 0
    property int activeNavigationCurrentNumber: 0
    property bool activeNavigationEditable: false
    property bool activeNavigationKnown: false
    property var openActiveNavigationAtNumber: function (number) {}
    property bool collectionControlsVisible: false
    property bool zoomOverflowFocusPending: false
    property Item toolbarOverflowButtonAnchor: null
    readonly property Item applicationMenuButtonAnchor: applicationMenuCoordinator.buttonAnchor
    readonly property int controlSpacing: compact ? Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2)) : Kirigami.Units.smallSpacing
    readonly property int edgeMargin: controlSpacing
    readonly property int fitModeSelection: imageDocument?.fitModeSelection ?? KiriImageDocument.Fit
    readonly property bool fitMenuButtonTextVisible: width >= Kirigami.Units.gridUnit * 40
    readonly property bool interactionActive: textInputCoordinator.active || applicationMenuCoordinator.open
    readonly property int presentedFitModeSelection: fitModeForActionId(retainedPresentation.presentedFitActionId)
    readonly property bool presentedImageReady: retainedPresentation.presentedImageReady
    readonly property bool presentedZoomEditable: retainedPresentation.presentedZoomEditable
    readonly property bool presentedZoomPercentAvailable: retainedPresentation.presentedZoomPercentAvailable
    readonly property bool presentedZoomPercentKnown: retainedPresentation.presentedZoomPercentKnown
    readonly property real presentedZoomPercent: retainedPresentation.presentedZoomPercent
    readonly property int presentedZoomMinimumManualPercent: retainedPresentation.presentedZoomMinimumManualPercent
    readonly property int presentedZoomMaximumManualPercent: retainedPresentation.presentedZoomMaximumManualPercent
    readonly property int toolbarVerticalPadding: controlSpacing
    readonly property var imageToolbarControls: (retainedPresentation.collectionControlsVisible ? [rightToLeftToolbarAction, twoPageToolbarAction] : []).concat([fitMenuAction, zoomLevelAction])
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

    function focusPageNumberInput() {
        return pageNavigation.focusPageNumberInput();
    }

    function focusZoomInput() {
        if (!retainedPresentation.zoomInteractionEnabled) {
            return false;
        }

        if (actionToolLayout.hiddenActions.includes(zoomLevelAction)) {
            if (root.toolbarOverflowButtonAnchor === null) {
                return false;
            }
            root.zoomOverflowFocusPending = true;
            toolbarOverflowMenu.popup(root.toolbarOverflowButtonAnchor, 0, root.toolbarOverflowButtonAnchor.height);
            return true;
        }

        zoomLevelAction.focusRequested();
        return true;
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

    function fitModeActionId(zoomMode) {
        if (zoomMode === KiriImageDocument.FitHeight) {
            return KiriViewApplication.ViewFitHeightAction;
        }
        if (zoomMode === KiriImageDocument.FitWidth) {
            return KiriViewApplication.ViewFitWidthAction;
        }
        return KiriViewApplication.ViewFitAction;
    }

    function fitModeForActionId(actionId) {
        if (actionId === KiriViewApplication.ViewFitHeightAction) {
            return KiriImageDocument.FitHeight;
        }
        if (actionId === KiriViewApplication.ViewFitWidthAction) {
            return KiriImageDocument.FitWidth;
        }
        return KiriImageDocument.Fit;
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
        if (retainedPresentation.actionInteractionEnabled(root.fitModeActionId(zoomMode), action?.enabled ?? false) && action !== null && action !== undefined && action.enabled) {
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

        collectionControlsVisibleFallback: root.collectionControlsVisible
        fitActionId: root.fitModeActionId(root.fitModeSelection)
        fitEnabledFallback: root.fitModeAction(root.fitModeSelection)?.enabled ?? false
        provider: root.navigationPresentationProvider
        rightToLeftActionId: KiriViewApplication.ViewToggleRightToLeftReadingAction
        rightToLeftCheckedFallback: root.actions.rightToLeftReadingAction?.checked ?? false
        rightToLeftEnabledFallback: root.actions.rightToLeftReadingAction?.enabled ?? false
        twoPageActionId: KiriViewApplication.ViewToggleTwoPageModeAction
        twoPageCheckedFallback: root.actions.twoPageModeAction?.checked ?? false
        twoPageEnabledFallback: root.actions.twoPageModeAction?.enabled ?? false
        zoomEditableFallback: root.zoomEditable
        zoomMaximumManualPercentFallback: root.maximumManualZoomPercent
        zoomMinimumManualPercentFallback: root.minimumManualZoomPercent
        zoomPercentAvailableFallback: root.zoomPercentAvailable
        zoomPercentFallback: root.zoomPercent
        zoomPercentKnownFallback: root.zoomPercentKnown
    }

    ImageToolBarTextInputCoordinator {
        id: textInputCoordinator

        onFocusReturnRequested: root.textInputFocusReturnRequested()
    }

    ImageToolBarMenuCoordinator {
        id: applicationMenuCoordinator

        actionSurface: actionToolLayout
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

    component ImageToolbarToggleButton: Controls.Control {
        id: toggleButton

        required property string controlObjectName
        required property bool presentationChecked
        required property bool presentationEnabled
        required property bool interactionEnabled
        required property var representedAction
        property bool textVisible: true
        readonly property bool checked: presentationChecked
        readonly property int display: textVisible ? Controls.AbstractButton.TextBesideIcon : Controls.AbstractButton.IconOnly
        readonly property string iconName: representedAction?.icon.name ?? ""
        readonly property string text: representedAction?.text ?? ""

        objectName: controlObjectName

        enabled: presentationEnabled
        focusPolicy: interactionEnabled ? Qt.StrongFocus : Qt.NoFocus
        implicitHeight: toggleContent.implicitHeight + topPadding + bottomPadding
        implicitWidth: toggleContent.implicitWidth + leftPadding + rightPadding
        bottomPadding: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: Kirigami.Units.smallSpacing

        Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.MenuItem
        Kirigami.MnemonicData.enabled: interactionEnabled && visible && textVisible
        Kirigami.MnemonicData.label: text

        Accessible.name: Kirigami.MnemonicData.plainTextLabel
        Accessible.role: Accessible.CheckBox
        Accessible.checked: presentationChecked
        Accessible.ignored: !interactionEnabled
        Accessible.onPressAction: activate()

        Controls.ToolTip.text: representedAction?.tooltip ?? Kirigami.MnemonicData.plainTextLabel
        Controls.ToolTip.visible: interactionEnabled && toggleHover.hovered && Controls.ToolTip.text.length > 0 && !toggleTap.pressed && !Kirigami.Settings.hasTransientTouchInput

        function activate() {
            if (interactionEnabled && (representedAction?.enabled ?? false)) {
                representedAction.trigger();
            }
        }

        onInteractionEnabledChanged: {
            if (!interactionEnabled) {
                focus = false;
            }
        }
        onVisibleChanged: {
            if (!visible) {
                focus = false;
            }
        }

        Keys.onPressed: event => {
            if (toggleButton.visible && toggleButton.interactionEnabled && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                toggleButton.activate();
                event.accepted = true;
            }
        }

        background: Rectangle {
            color: {
                if (toggleButton.presentationChecked) {
                    return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.32);
                }
                if (!toggleButton.interactionEnabled) {
                    return "transparent";
                }
                if (toggleTap.pressed) {
                    return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2);
                }
                if (toggleHover.hovered || toggleButton.visualFocus) {
                    return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.1);
                }
                return "transparent";
            }
            radius: Kirigami.Units.cornerRadius
        }

        contentItem: RowLayout {
            id: toggleContent

            layoutDirection: toggleButton.mirrored ? Qt.RightToLeft : Qt.LeftToRight
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredWidth: Layout.preferredHeight
                source: toggleButton.iconName
            }

            Controls.Label {
                Layout.alignment: Qt.AlignVCenter
                color: toggleButton.presentationEnabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                text: toggleButton.Kirigami.MnemonicData.richTextLabel
                textFormat: Text.RichText
                visible: toggleButton.textVisible
            }
        }

        HoverHandler {
            id: toggleHover

            enabled: toggleButton.interactionEnabled
        }

        TapHandler {
            id: toggleTap

            acceptedButtons: Qt.LeftButton
            enabled: toggleButton.interactionEnabled

            onTapped: toggleButton.activate()
        }

        Shortcut {
            context: Qt.WindowShortcut
            enabled: toggleButton.interactionEnabled && toggleButton.visible && toggleButton.textVisible
            sequence: toggleButton.Kirigami.MnemonicData.sequence

            onActivated: toggleButton.activate()
        }
    }

    component ImageToolbarPlacementAction: Kirigami.Action {
        property Component iconDisplayComponent
        property bool presentationChecked: false
        property bool presentationEnabled: false

        signal focusRequested
    }

    component ImageToolbarZoomIconButton: Controls.Control {
        id: zoomIconButton

        required property bool interactionEnabled
        required property bool presentationEnabled
        required property Component zoomControlsComponent
        property bool zoomFocusPending: false
        readonly property ImageZoomControls popupControls: zoomPopupLoader.item as ImageZoomControls

        function focusLoadedZoomInput() {
            if (zoomFocusPending && popupControls !== null && popupControls.focusZoomInput()) {
                zoomFocusPending = false;
            }
        }

        function focusZoomInput() {
            if (!visible || !interactionEnabled) {
                return false;
            }

            zoomFocusPending = true;
            zoomPopup.open();
            Qt.callLater(focusLoadedZoomInput);
            return true;
        }

        enabled: presentationEnabled
        focusPolicy: interactionEnabled ? Qt.StrongFocus : Qt.NoFocus
        implicitHeight: zoomIcon.implicitHeight + topPadding + bottomPadding
        implicitWidth: zoomIcon.implicitWidth + leftPadding + rightPadding
        bottomPadding: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: Kirigami.Units.smallSpacing

        Accessible.name: root.zoomLevelAction.text
        Accessible.role: Accessible.ButtonMenu
        Accessible.ignored: !interactionEnabled
        Accessible.onPressAction: {
            if (interactionEnabled) {
                zoomPopup.open();
            }
        }

        Controls.ToolTip.text: root.zoomLevelAction.tooltip
        Controls.ToolTip.visible: interactionEnabled && zoomIconHover.hovered && !zoomIconTap.pressed && !zoomPopup.visible && !Kirigami.Settings.hasTransientTouchInput

        onInteractionEnabledChanged: {
            if (!interactionEnabled) {
                zoomFocusPending = false;
                focus = false;
                zoomPopup.close();
            }
        }
        onVisibleChanged: {
            if (!visible) {
                zoomFocusPending = false;
                if (popupControls?.textInputActive ?? false) {
                    popupControls.cancelEditing(true);
                }
                zoomPopup.close();
                focus = false;
            }
        }

        Keys.onPressed: event => {
            if (zoomIconButton.visible && zoomIconButton.interactionEnabled && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                zoomPopup.open();
                event.accepted = true;
            }
        }

        Connections {
            target: root.zoomLevelAction

            function onFocusRequested() {
                zoomIconButton.focusZoomInput();
            }
        }

        background: Rectangle {
            color: {
                if (!zoomIconButton.interactionEnabled) {
                    return "transparent";
                }
                if (zoomIconTap.pressed) {
                    return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2);
                }
                if (zoomIconHover.hovered || zoomIconButton.visualFocus) {
                    return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.1);
                }
                return "transparent";
            }
            radius: Kirigami.Units.cornerRadius
        }

        contentItem: Kirigami.Icon {
            id: zoomIcon

            implicitHeight: Kirigami.Units.iconSizes.smallMedium
            implicitWidth: implicitHeight
            source: root.zoomLevelAction.icon.name
        }

        HoverHandler {
            id: zoomIconHover

            enabled: zoomIconButton.interactionEnabled
        }

        TapHandler {
            id: zoomIconTap

            acceptedButtons: Qt.LeftButton
            enabled: zoomIconButton.interactionEnabled

            onTapped: zoomPopup.open()
        }

        Controls.Popup {
            id: zoomPopup

            closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
            popupType: Controls.Popup.Item
            y: zoomIconButton.height

            onClosed: zoomIconButton.zoomFocusPending = false

            contentItem: Loader {
                id: zoomPopupLoader

                active: zoomPopup.visible
                sourceComponent: zoomIconButton.zoomControlsComponent

                onLoaded: Qt.callLater(zoomIconButton.focusLoadedZoomInput)
            }
        }
    }

    Component {
        id: zoomControlsDisplayComponent

        ImageZoomControls {
            id: zoomControls

            compact: root.compact
            imageDocument: root.imageDocument
            imageReady: root.presentedImageReady
            interactionEnabled: retainedPresentation.zoomInteractionEnabled
            maximumManualZoomPercent: root.presentedZoomMaximumManualPercent
            minimumManualZoomPercent: root.presentedZoomMinimumManualPercent
            presentationEditable: root.presentedZoomEditable
            readOnlyDisplayMode: root.presentedZoomPercentAvailable && !root.presentedZoomEditable
            readOnlyPercent: Math.round(root.presentedZoomPercent)
            readOnlyPercentKnown: root.presentedZoomPercentKnown
            presentationEnabled: retainedPresentation.zoomAppearanceEnabled
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

            Connections {
                target: root.zoomLevelAction

                function onFocusRequested() {
                    if (zoomControls.visible) {
                        zoomControls.focusZoomInput();
                    }
                }
            }
        }
    }

    readonly property ImageToolbarPlacementAction rightToLeftToolbarAction: ImageToolbarPlacementAction {
        checkable: root.actions.rightToLeftReadingAction?.checkable ?? true
        checked: root.actions.rightToLeftReadingAction?.checked ?? false
        displayComponent: ImageToolbarToggleButton {
            controlObjectName: "rightToLeftToolbarButton"
            interactionEnabled: retainedPresentation.rightToLeftInteractionEnabled
            presentationChecked: retainedPresentation.rightToLeftAppearanceChecked
            presentationEnabled: retainedPresentation.rightToLeftAppearanceEnabled
            representedAction: root.rightToLeftToolbarAction
        }
        iconDisplayComponent: ImageToolbarToggleButton {
            controlObjectName: "rightToLeftToolbarIconButton"
            interactionEnabled: retainedPresentation.rightToLeftInteractionEnabled
            presentationChecked: retainedPresentation.rightToLeftAppearanceChecked
            presentationEnabled: retainedPresentation.rightToLeftAppearanceEnabled
            representedAction: root.rightToLeftToolbarAction
            textVisible: false
        }
        displayHint: root.actions.rightToLeftReadingAction?.displayHint ?? Kirigami.DisplayHint.KeepVisible
        enabled: retainedPresentation.rightToLeftInteractionEnabled
        icon.name: root.actions.rightToLeftReadingAction?.icon.name ?? ""
        presentationChecked: retainedPresentation.rightToLeftAppearanceChecked
        presentationEnabled: retainedPresentation.rightToLeftAppearanceEnabled
        shortcut: ""
        text: root.actions.rightToLeftReadingAction?.text ?? ""
        tooltip: root.actions.rightToLeftReadingAction?.tooltip ?? text
        visible: root.actions.rightToLeftReadingAction?.visible ?? true

        onTriggered: {
            if (enabled && (root.actions.rightToLeftReadingAction?.enabled ?? false)) {
                root.actions.rightToLeftReadingAction.trigger();
            }
        }
    }

    readonly property ImageToolbarPlacementAction twoPageToolbarAction: ImageToolbarPlacementAction {
        checkable: root.actions.twoPageModeAction?.checkable ?? true
        checked: root.actions.twoPageModeAction?.checked ?? false
        displayComponent: ImageToolbarToggleButton {
            controlObjectName: "twoPageToolbarButton"
            interactionEnabled: retainedPresentation.twoPageInteractionEnabled
            presentationChecked: retainedPresentation.twoPageAppearanceChecked
            presentationEnabled: retainedPresentation.twoPageAppearanceEnabled
            representedAction: root.twoPageToolbarAction
        }
        iconDisplayComponent: ImageToolbarToggleButton {
            controlObjectName: "twoPageToolbarIconButton"
            interactionEnabled: retainedPresentation.twoPageInteractionEnabled
            presentationChecked: retainedPresentation.twoPageAppearanceChecked
            presentationEnabled: retainedPresentation.twoPageAppearanceEnabled
            representedAction: root.twoPageToolbarAction
            textVisible: false
        }
        displayHint: root.actions.twoPageModeAction?.displayHint ?? Kirigami.DisplayHint.KeepVisible
        enabled: retainedPresentation.twoPageInteractionEnabled
        icon.name: root.actions.twoPageModeAction?.icon.name ?? ""
        presentationChecked: retainedPresentation.twoPageAppearanceChecked
        presentationEnabled: retainedPresentation.twoPageAppearanceEnabled
        shortcut: ""
        text: root.actions.twoPageModeAction?.text ?? ""
        tooltip: root.actions.twoPageModeAction?.tooltip ?? text
        visible: root.actions.twoPageModeAction?.visible ?? true

        onTriggered: {
            if (enabled && (root.actions.twoPageModeAction?.enabled ?? false)) {
                root.actions.twoPageModeAction.trigger();
            }
        }
    }

    readonly property ImageToolbarPlacementAction zoomLevelAction: ImageToolbarPlacementAction {
        displayComponent: zoomControlsDisplayComponent
        iconDisplayComponent: ImageToolbarZoomIconButton {
            interactionEnabled: retainedPresentation.zoomInteractionEnabled
            presentationEnabled: retainedPresentation.zoomAppearanceEnabled
            zoomControlsComponent: zoomControlsDisplayComponent
        }
        displayHint: Kirigami.DisplayHint.KeepVisible
        enabled: retainedPresentation.zoomInteractionEnabled
        icon.name: "zoom-original-symbolic"
        presentationEnabled: retainedPresentation.zoomAppearanceEnabled
        text: KI18n.i18nc("@action", "Zoom")
        tooltip: text
    }

    readonly property ImageToolbarPlacementAction fitMenuAction: ImageToolbarPlacementAction {
        displayComponent: FitModeMenuButton {
            action: root.fitMenuAction
            fitAction: root.actions.fitAction
            fitHeightAction: root.actions.fitHeightAction
            fitHeightMode: KiriImageDocument.FitHeight
            fitMode: KiriImageDocument.Fit
            fitWidthAction: root.actions.fitWidthAction
            fitWidthMode: KiriImageDocument.FitWidth
            fitModeSelection: root.presentedFitModeSelection
            interactionEnabled: retainedPresentation.fitInteractionEnabled
            presentationEnabled: retainedPresentation.fitAppearanceEnabled
            textVisible: root.fitMenuButtonTextVisible

            onFitModeTriggered: function (zoomMode) {
                root.triggerFitMode(zoomMode);
            }
        }
        iconDisplayComponent: FitModeMenuButton {
            action: root.fitMenuAction
            fitAction: root.actions.fitAction
            fitHeightAction: root.actions.fitHeightAction
            fitHeightMode: KiriImageDocument.FitHeight
            fitMode: KiriImageDocument.Fit
            fitWidthAction: root.actions.fitWidthAction
            fitWidthMode: KiriImageDocument.FitWidth
            fitModeSelection: root.presentedFitModeSelection
            interactionEnabled: retainedPresentation.fitInteractionEnabled
            presentationEnabled: retainedPresentation.fitAppearanceEnabled
            textVisible: false

            onFitModeTriggered: function (zoomMode) {
                root.triggerFitMode(zoomMode);
            }
        }
        displayHint: Kirigami.DisplayHint.KeepVisible
        enabled: retainedPresentation.fitInteractionEnabled
        icon.name: root.fitModeIconName(root.presentedFitModeSelection)
        presentationEnabled: retainedPresentation.fitAppearanceEnabled
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
            onVisibleChanged: {
                root.updateApplicationMenuButtonAnchor(applicationMenuButton);
                if (!visible) {
                    focus = false;
                    skipNextClick = false;
                }
            }
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

    Controls.Menu {
        id: toolbarOverflowMenu

        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        popupType: Controls.Popup.Item

        onClosed: root.zoomOverflowFocusPending = false
        onOpened: {
            if (root.zoomOverflowFocusPending) {
                Qt.callLater(function () {
                    if (toolbarOverflowMenu.visible) {
                        root.zoomLevelAction.focusRequested();
                    }
                    root.zoomOverflowFocusPending = false;
                });
            }
        }

        Instantiator {
            model: root.toolbarActions

            delegate: Loader {
                required property var modelData

                readonly property bool actionHidden: actionToolLayout.hiddenActions.includes(modelData)
                readonly property bool actionVisible: modelData?.visible ?? true

                active: actionHidden && actionVisible && toolbarOverflowMenu.visible
                height: visible ? implicitHeight : 0
                sourceComponent: modelData?.displayComponent ?? null
                visible: actionHidden && actionVisible
            }

            onObjectAdded: (index, object) => toolbarOverflowMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => toolbarOverflowMenu.removeItem(object)
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

        KirigamiLayouts.ToolBarLayout {
            id: actionToolLayout

            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: Math.max(Kirigami.Units.gridUnit * 2, actionToolLayout.minimumWidth)

            actions: root.toolbarActions
            alignment: Qt.AlignRight
            layoutDirection: root.mirrored ? Qt.RightToLeft : Qt.LeftToRight
            spacing: root.controlSpacing

            onHiddenActionsChanged: {
                if (hiddenActions.length === 0) {
                    toolbarOverflowMenu.dismiss();
                }
            }

            fullDelegate: Controls.ToolButton {
                action: KirigamiLayouts.ToolBarLayout.action as Templates.Action
                display: Controls.AbstractButton.TextBesideIcon
                flat: true
            }

            iconDelegate: Loader {
                readonly property var toolbarAction: KirigamiLayouts.ToolBarLayout.action

                sourceComponent: toolbarAction?.iconDisplayComponent ?? toolbarAction?.displayComponent ?? null
            }

            separatorDelegate: Controls.ToolSeparator {}

            moreButton: Controls.ToolButton {
                objectName: "toolbarOverflowButton"

                Accessible.name: KI18n.i18nc("@info:tooltip", "More toolbar actions")
                Accessible.role: Accessible.ButtonMenu

                display: Controls.AbstractButton.IconOnly
                flat: true
                icon.name: "overflow-menu"

                Controls.ToolTip.text: Accessible.name
                Controls.ToolTip.visible: hovered && !pressed && !toolbarOverflowMenu.visible && !Kirigami.Settings.hasTransientTouchInput

                Component.onCompleted: root.toolbarOverflowButtonAnchor = this
                Component.onDestruction: {
                    if (root.toolbarOverflowButtonAnchor === this) {
                        root.toolbarOverflowButtonAnchor = null;
                    }
                }

                onClicked: toolbarOverflowMenu.popup(this, 0, height)
                onVisibleChanged: {
                    if (!visible) {
                        toolbarOverflowMenu.dismiss();
                        focus = false;
                    }
                }
            }
        }
    }
}
