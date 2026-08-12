// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardDialog {
    id: root

    required property KiriViewApplication application
    property int editingActionId: KiriViewApplication.ActionCount
    property int editingActivationScope: KiriViewApplication.ProgramWideShortcutScope
    property string editingActionText: ""
    property string editingScopeText: ""
    readonly property real dialogMargin: Kirigami.Units.gridUnit
    readonly property real maximumDialogWidth: Kirigami.Units.gridUnit * 38
    readonly property real maximumDialogHeight: Kirigami.Units.gridUnit * 34
    readonly property real minimumDialogWidth: Kirigami.Units.gridUnit * 18
    readonly property real availableDialogWidth: parent ? Math.max(minimumDialogWidth, parent.width - dialogMargin * 2) : maximumDialogWidth
    readonly property real availableDialogHeight: parent ? Math.max(Kirigami.Units.gridUnit * 14, parent.height - dialogMargin * 2) : maximumDialogHeight
    readonly property real cappedDialogHeight: Math.min(availableDialogHeight, maximumDialogHeight)
    readonly property real formMaximumWidth: Kirigami.Units.gridUnit * 36
    readonly property real headerHeightBudget: header ? header.implicitHeight : 0
    readonly property real footerHeightBudget: footer ? footer.implicitHeight : 0
    readonly property real listViewportHeight: Math.max(Kirigami.Units.gridUnit * 10, cappedDialogHeight - headerHeightBudget - footerHeightBudget)

    function editShortcutSlot(actionId, activationScope, actionText, scopeText, portableShortcutTexts) {
        root.editingActionId = actionId;
        root.editingActivationScope = activationScope;
        root.editingActionText = actionText;
        root.editingScopeText = scopeText;
        shortcutTextArea.text = portableShortcutTexts.join("\n");
        editorError.visible = false;
        shortcutEditorDialog.open();
    }

    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnReleaseOutside
    focus: true
    height: Math.min(implicitHeight, cappedDialogHeight)
    parent: Controls.Overlay.overlay
    standardButtons: Controls.Dialog.Close
    title: KI18n.i18nc("@title:window", "Configure Keyboard Shortcuts")
    width: Math.min(maximumDialogWidth, availableDialogWidth)

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            bottomPadding: 0
            elide: Text.ElideRight
            leftPadding: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.smallSpacing
            text: root.title
            topPadding: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
        }

        Controls.ToolButton {
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            Layout.rightMargin: Kirigami.Units.smallSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Accessible.name: text
            display: Controls.AbstractButton.IconOnly
            icon.name: "dialog-close-symbolic"
            text: KI18n.i18nc("@action:button", "Close")

            onClicked: root.close()
        }
    }

    Controls.ScrollView {
        id: scrollView

        objectName: "shortcutConfigurationScrollView"

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.maximumHeight: Layout.preferredHeight
        Layout.preferredHeight: Math.min(shortcutList.implicitHeight, root.listViewportHeight)
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        clip: true
        contentHeight: shortcutList.implicitHeight
        contentWidth: availableWidth

        ColumnLayout {
            id: shortcutList

            objectName: "shortcutConfigurationList"

            spacing: 0
            width: scrollView.availableWidth

            Repeater {
                model: root.application.shortcutConfigurationModel

                delegate: ColumnLayout {
                    id: shortcutRow

                    required property int actionId
                    required property string actionText
                    required property int activationScope
                    required property bool categoryFirst
                    required property bool categoryLast
                    required property string categoryText
                    required property var portableShortcutTexts
                    required property string scopeText
                    required property string shortcutText

                    Layout.fillWidth: true
                    spacing: 0

                    FormCard.FormHeader {
                        Layout.fillWidth: true
                        maximumWidth: root.formMaximumWidth
                        title: shortcutRow.categoryText
                        visible: shortcutRow.categoryFirst
                    }

                    Controls.ItemDelegate {
                        objectName: "shortcutConfigurationRow"

                        Layout.fillWidth: true
                        Accessible.description: shortcutRow.scopeText

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Controls.Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: shortcutRow.actionText
                                }

                                Controls.Label {
                                    Layout.fillWidth: true
                                    color: Kirigami.Theme.disabledTextColor
                                    elide: Text.ElideRight
                                    font: Kirigami.Theme.smallFont
                                    text: shortcutRow.scopeText
                                }
                            }

                            Controls.Label {
                                Layout.maximumWidth: Kirigami.Units.gridUnit * 12
                                elide: Text.ElideRight
                                font.family: Kirigami.Theme.fixedWidthFont.family
                                text: shortcutRow.shortcutText
                            }

                            Controls.ToolButton {
                                Accessible.name: text
                                display: Controls.AbstractButton.IconOnly
                                icon.name: "configure"
                                text: KI18n.i18nc("@action:button", "Edit Shortcut")

                                onClicked: root.editShortcutSlot(shortcutRow.actionId, shortcutRow.activationScope, shortcutRow.actionText, shortcutRow.scopeText, shortcutRow.portableShortcutTexts)
                            }
                        }

                        onClicked: root.editShortcutSlot(shortcutRow.actionId, shortcutRow.activationScope, shortcutRow.actionText, shortcutRow.scopeText, shortcutRow.portableShortcutTexts)
                    }

                    FormCard.FormDelegateSeparator {
                        Layout.fillWidth: true
                        visible: !shortcutRow.categoryLast
                    }
                }
            }
        }
    }

    Controls.Dialog {
        id: shortcutEditorDialog

        objectName: "shortcutSlotEditorDialog"

        anchors.centerIn: parent
        closePolicy: Controls.Popup.CloseOnEscape
        modal: true
        parent: Controls.Overlay.overlay
        title: root.editingActionText
        width: Math.min(Kirigami.Units.gridUnit * 26, root.availableDialogWidth)

        ColumnLayout {
            width: parent.width

            Controls.Label {
                Layout.fillWidth: true
                text: root.editingScopeText
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18nc("@info", "Enter one shortcut per line using portable key names, for example Ctrl+O. Leave the field empty to unassign this slot.")
                wrapMode: Text.Wrap
            }

            Controls.ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 8

                Controls.TextArea {
                    id: shortcutTextArea

                    objectName: "shortcutConfigurationTextArea"

                    font.family: Kirigami.Theme.fixedWidthFont.family
                    placeholderText: KI18n.i18nc("@info:placeholder", "Ctrl+O")
                    wrapMode: TextEdit.NoWrap
                }
            }

            Controls.Label {
                id: editorError

                Layout.fillWidth: true
                color: Kirigami.Theme.negativeTextColor
                text: KI18n.i18nc("@info", "One or more shortcuts are invalid for this scope.")
                visible: false
                wrapMode: Text.Wrap
            }
        }

        footer: Controls.DialogButtonBox {
            Controls.Button {
                text: KI18n.i18nc("@action:button", "Apply")

                Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.AcceptRole
            }

            standardButtons: Controls.DialogButtonBox.Cancel

            onAccepted: {
                const shortcutTexts = shortcutTextArea.text.split(/\r?\n/);
                if (root.application.setShortcutTextsForId(root.editingActionId, root.editingActivationScope, shortcutTexts)) {
                    shortcutEditorDialog.close();
                } else {
                    editorError.visible = true;
                }
            }
            onRejected: shortcutEditorDialog.close()
        }
    }
}
