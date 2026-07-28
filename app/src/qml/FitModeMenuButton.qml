// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami

Controls.Control {
    id: root

    objectName: "fitModeMenuButton"

    required property var action
    required property var fitAction
    required property var fitHeightAction
    required property var fitWidthAction
    required property int fitMode
    required property int fitHeightMode
    required property int fitWidthMode
    required property int fitModeSelection
    required property bool presentationEnabled
    property bool textVisible: true
    property bool interactionEnabled: false

    readonly property int display: textVisible ? Controls.AbstractButton.TextBesideIcon : Controls.AbstractButton.IconOnly
    readonly property string iconName: action?.icon.name ?? ""
    readonly property string menuTooltip: KI18n.i18nc("@info:tooltip", "Choose fit mode")
    readonly property string text: action?.text ?? ""
    readonly property string tooltip: menuTooltip

    signal fitModeTriggered(int zoomMode)

    function triggerFitMode(zoomMode) {
        root.fitModeTriggered(zoomMode);
        fitMenu.dismiss();
    }

    enabled: presentationEnabled
    focusPolicy: interactionEnabled ? Qt.StrongFocus : Qt.NoFocus
    implicitHeight: contentLayout.implicitHeight + topPadding + bottomPadding
    implicitWidth: contentLayout.implicitWidth + leftPadding + rightPadding
    Layout.alignment: Qt.AlignVCenter
    bottomPadding: Kirigami.Units.smallSpacing
    leftPadding: Kirigami.Units.smallSpacing
    rightPadding: Kirigami.Units.smallSpacing
    topPadding: Kirigami.Units.smallSpacing

    Accessible.name: menuTooltip
    Accessible.role: Accessible.ButtonMenu
    Accessible.ignored: !visible || !interactionEnabled
    Accessible.onPressAction: {
        if (interactionEnabled) {
            fitMenu.open();
        }
    }

    Controls.ToolTip.text: menuTooltip
    Controls.ToolTip.visible: interactionEnabled && hoverHandler.hovered && Controls.ToolTip.text.length > 0 && !fitMenu.visible && !tapHandler.pressed && !Kirigami.Settings.hasTransientTouchInput

    onInteractionEnabledChanged: {
        if (!interactionEnabled) {
            fitMenu.dismiss();
            focus = false;
        }
    }
    onVisibleChanged: {
        if (!visible) {
            fitMenu.dismiss();
            focus = false;
        }
    }

    Keys.onPressed: event => {
        if (root.visible && root.interactionEnabled && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            fitMenu.open();
            event.accepted = true;
        }
    }

    background: Rectangle {
        color: {
            if (!root.interactionEnabled) {
                return "transparent";
            }
            if (tapHandler.pressed) {
                return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2);
            }
            if (hoverHandler.hovered || root.visualFocus) {
                return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.1);
            }
            return "transparent";
        }
        radius: Kirigami.Units.cornerRadius
    }

    contentItem: RowLayout {
        id: contentLayout

        layoutDirection: root.mirrored ? Qt.RightToLeft : Qt.LeftToRight
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredWidth: Layout.preferredHeight
            source: root.iconName
        }

        Controls.Label {
            Layout.alignment: Qt.AlignVCenter
            color: root.presentationEnabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
            text: root.text
            visible: root.textVisible
        }
    }

    HoverHandler {
        id: hoverHandler

        enabled: root.interactionEnabled
    }

    TapHandler {
        id: tapHandler

        acceptedButtons: Qt.LeftButton
        enabled: root.interactionEnabled

        onTapped: fitMenu.open()
    }

    Controls.Menu {
        id: fitMenu

        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        popupType: Controls.Popup.Item
        y: root.height

        Controls.MenuItem {
            objectName: "fitToWindowMenuItem"

            checkable: true
            checked: root.fitModeSelection === root.fitMode
            enabled: root.fitAction?.enabled ?? false
            icon.name: root.fitAction?.icon.name ?? "zoom-fit-best-symbolic"
            text: KI18n.i18nc("@action:inmenu", "Fit to Window")

            onTriggered: root.triggerFitMode(root.fitMode)
        }

        Controls.MenuItem {
            objectName: "fitWidthMenuItem"

            checkable: true
            checked: root.fitModeSelection === root.fitWidthMode
            enabled: root.fitWidthAction?.enabled ?? false
            icon.name: "zoom-fit-width"
            text: KI18n.i18nc("@action:inmenu", "Fit Width")

            onTriggered: root.triggerFitMode(root.fitWidthMode)
        }

        Controls.MenuItem {
            objectName: "fitHeightMenuItem"

            checkable: true
            checked: root.fitModeSelection === root.fitHeightMode
            enabled: root.fitHeightAction?.enabled ?? false
            icon.name: "zoom-fit-height"
            text: KI18n.i18nc("@action:inmenu", "Fit Height")

            onTriggered: root.triggerFitMode(root.fitHeightMode)
        }
    }
}
