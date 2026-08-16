// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.hnjae.kiriview
import org.kde.kirigami as Kirigami

RowLayout {
    id: root

    objectName: "imageZoomControls"

    required property KiriImageDocument imageDocument
    required property bool imageReady
    required property int minimumManualZoomPercent
    required property int maximumManualZoomPercent
    required property real zoomStepFactor
    property bool compact: false
    property bool readOnlyDisplayMode: false
    property bool readOnlyPercentKnown: false
    property int readOnlyPercent: 0
    property bool presentationEnabled: zoomEditable
    property bool interactionEnabled: zoomEditable
    property bool presentationEditable: zoomEditable
    property bool zoomPercentAvailable: readOnlyDisplayMode || imageReady
    property bool zoomPercentKnown: readOnlyDisplayMode ? readOnlyPercentKnown : imageReady
    property real zoomPercent: readOnlyDisplayMode ? readOnlyPercent : imageDocument.zoomPercent
    property bool zoomEditable: !readOnlyDisplayMode && imageReady
    readonly property int controlSpacing: compact ? Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2)) : Kirigami.Units.smallSpacing
    readonly property real toolbarWheelZoomStepScale: 0.5
    readonly property bool textInputActive: textInputFocused()

    signal editingCompleted(bool returnViewerFocus)

    function cancelEditing(returnViewerFocus) {
        if (textInputFocused()) {
            zoomSpinBox.cancelEditing(returnViewerFocus);
        }
    }

    function commitEditing(returnViewerFocus) {
        if (textInputFocused()) {
            zoomSpinBox.commitEditing(returnViewerFocus);
        }
    }

    function focusZoomInput() {
        if (!visible || !zoomSpinBox.semanticInputAvailable || !zoomTextInput.enabled) {
            return false;
        }

        zoomTextInput.forceActiveFocus(Qt.ShortcutFocusReason);
        if (!zoomTextInput.activeFocus) {
            return false;
        }

        zoomTextInput.selectAll();
        return true;
    }

    spacing: controlSpacing

    function textInputFocused() {
        return zoomSpinBox.activeFocus || zoomTextInput.activeFocus;
    }

    onInteractionEnabledChanged: {
        if (!interactionEnabled) {
            decreaseRepeatTimer.stop();
            increaseRepeatTimer.stop();
            cancelEditing(false);
        }
    }
    onVisibleChanged: {
        if (!visible) {
            decreaseRepeatTimer.stop();
            increaseRepeatTimer.stop();
            cancelEditing(true);
        }
    }

    function handleZoomWheel(wheel) {
        if (!interactionEnabled || !presentationEditable) {
            wheel.accepted = false;
            return;
        }

        const stepCount = wheelZoomPolicy.stepCount(wheel);
        if (stepCount === 0) {
            wheel.accepted = false;
            return;
        }

        root.imageDocument.requestZoomByStepAtCenter(stepCount);
        wheel.accepted = true;
    }

    ZoomWheelStepPolicy {
        id: wheelZoomPolicy

        stepScale: root.toolbarWheelZoomStepScale
    }

    Controls.Control {
        id: zoomPresentationSurface

        objectName: "zoomPresentationSurface"

        Accessible.ignored: !root.readOnlyDisplayMode || !root.zoomPercentAvailable
        Accessible.name: zoomSpinBox.formattedDisplayText.trim() + " %"
        Accessible.role: Accessible.StaticText

        readonly property int stepperWidth: Math.max(Kirigami.Units.gridUnit, Math.round(implicitHeight / 2))

        enabled: root.presentationEnabled
        implicitHeight: Math.max(Kirigami.Units.gridUnit * 2, zoomTextMetrics.height + root.controlSpacing * 2)
        implicitWidth: Kirigami.Units.gridUnit * 5
        leftPadding: root.controlSpacing
        rightPadding: 0

        background: Rectangle {
            color: zoomPresentationSurface.palette.base
            radius: Kirigami.Units.cornerRadius

            border.color: zoomSpinBox.activeFocus || zoomTextInput.activeFocus ? zoomPresentationSurface.palette.highlight : zoomPresentationSurface.palette.mid
            border.width: 1
        }

        contentItem: Item {
            FocusScope {
                id: zoomSpinBox

                objectName: "zoomSpinBox"

                Accessible.ignored: true

                property bool completingEdit: false
                readonly property int zoomDisplayWidth: 5
                readonly property real zoomDisplayEpsilon: 0.001
                readonly property int zoomKiloThresholdPercent: 10000
                readonly property int zoomOverflowThresholdPercent: 1000000
                readonly property bool zoomValueAvailable: root.zoomPercentAvailable
                readonly property bool zoomValueKnown: root.zoomPercentKnown
                readonly property real rawZoomPercent: root.zoomPercent
                readonly property real numericZoomPercent: Number.isFinite(Number(rawZoomPercent)) ? Number(rawZoomPercent) : 0
                readonly property string formattedDisplayText: formattedZoomText(rawZoomPercent, zoomValueAvailable, zoomValueKnown)
                readonly property string editableDisplayText: plainZoomText(value)
                readonly property bool editable: root.presentationEditable
                readonly property int from: editable ? Math.min(root.minimumManualZoomPercent, Math.floor(numericZoomPercent)) : 0
                readonly property int stepSize: Math.max(1, Math.round(numericZoomPercent * (root.zoomStepFactor - 1)))
                readonly property int to: editable ? Math.max(root.maximumManualZoomPercent, Math.ceil(numericZoomPercent)) : Math.max(0, Math.round(numericZoomPercent))
                readonly property int value: zoomValueAvailable && zoomValueKnown ? Math.max(0, Math.round(numericZoomPercent)) : 0
                readonly property bool decreasePresentationAvailable: editable && value > from
                readonly property bool increasePresentationAvailable: editable && value < to
                readonly property bool semanticInputAvailable: root.presentationEnabled && root.interactionEnabled && root.presentationEditable

                anchors.fill: parent
                enabled: semanticInputAvailable
                z: 1

                function cancelEditing(returnViewerFocus) {
                    if (completingEdit) {
                        return;
                    }

                    completingEdit = true;
                    restoreZoomText();
                    clearEditingFocus();
                    completingEdit = false;
                    if (returnViewerFocus === true) {
                        root.editingCompleted(true);
                    }
                }

                function clearEditingFocus() {
                    zoomTextInput.focus = false;
                    zoomSpinBox.focus = false;
                }

                function commitEditing(returnViewerFocus) {
                    if (completingEdit) {
                        return;
                    }

                    completingEdit = true;
                    commitZoomText();
                    clearEditingFocus();
                    completingEdit = false;
                    if (returnViewerFocus === true) {
                        root.editingCompleted(true);
                    }
                }

                function commitZoomText() {
                    if (!semanticInputAvailable || !enabled) {
                        restoreZoomText();
                        return;
                    }

                    const zoomPercent = parsedZoomText();
                    if (Number.isFinite(zoomPercent)) {
                        root.imageDocument.requestManualZoomPercent(Math.round(zoomPercent));
                    }
                    restoreZoomText();
                }

                function decrease() {
                    requestStep(-1);
                }

                function formattedZoomText(rawPercent, valueAvailable, valueKnown) {
                    if (!valueAvailable) {
                        return paddedZoomText("-");
                    }

                    const percent = Number(rawPercent);
                    if (!valueKnown || !Number.isFinite(percent)) {
                        return paddedZoomText("?");
                    }

                    if (percent < zoomKiloThresholdPercent) {
                        const roundedPercent = Math.min(zoomKiloThresholdPercent - 1, Math.max(0, Math.round(percent)));
                        return paddedZoomText(roundedPercent.toString());
                    }

                    if (percent >= zoomOverflowThresholdPercent) {
                        return paddedZoomText("999k+");
                    }

                    const nearestKilo = Math.round(percent / 1000);
                    const nearestKiloPercent = nearestKilo * 1000;
                    if (Math.abs(percent - nearestKiloPercent) < zoomDisplayEpsilon) {
                        if (nearestKilo >= 1000) {
                            return paddedZoomText("999k+");
                        }
                        return paddedZoomText(nearestKilo.toString() + "k");
                    }

                    const kilo = Math.min(999, Math.floor(percent / 1000));
                    return paddedZoomText(kilo.toString() + "k+");
                }

                function increase() {
                    requestStep(1);
                }

                function paddedZoomText(text) {
                    let paddedText = text.toString();
                    while (paddedText.length < zoomDisplayWidth) {
                        paddedText = " " + paddedText;
                    }
                    return paddedText;
                }

                function parsedZoomText() {
                    const withoutPercent = zoomText().replace("%", "").trim();
                    if (withoutPercent.length === 0) {
                        return NaN;
                    }

                    const parsedValue = Number.fromLocaleString(Qt.locale(), withoutPercent);
                    return Number.isFinite(parsedValue) ? parsedValue : NaN;
                }

                function plainZoomText(value) {
                    return Number(value).toString();
                }

                function requestStep(stepCount) {
                    if (!semanticInputAvailable || !enabled) {
                        return;
                    }
                    if ((stepCount > 0 && !increasePresentationAvailable) || (stepCount < 0 && !decreasePresentationAvailable)) {
                        return;
                    }
                    root.imageDocument.requestZoomByStepAtCenter(stepCount);
                }

                function restoreZoomText() {
                    zoomTextInput.text = zoomSpinBox.formattedDisplayText;
                }

                function textFromValue(value, locale) {
                    return plainZoomText(value);
                }

                function valueFromText(text, locale) {
                    const withoutPercent = text.toString().replace("%", "").trim();
                    const parsedValue = Number.fromLocaleString(locale, withoutPercent);
                    return Number.isFinite(parsedValue) ? Math.round(parsedValue) : zoomSpinBox.value;
                }

                function zoomText() {
                    return zoomTextInput.text.toString();
                }

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    acceptedModifiers: Qt.NoModifier
                    blocking: true
                    enabled: zoomSpinBox.semanticInputAvailable && !root.textInputActive
                    target: null

                    onWheel: wheel => {
                        root.handleZoomWheel(wheel);
                    }
                }
            }

            RowLayout {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: stepperColumn.left
                anchors.top: parent.top
                spacing: zoomSuffixGapMetrics.advanceWidth

                TextMetrics {
                    id: zoomSuffixGapMetrics

                    font: Kirigami.Theme.fixedWidthFont
                    text: " "
                }

                TextMetrics {
                    id: zoomTextMetrics

                    font: Kirigami.Theme.fixedWidthFont
                    text: "999k+"
                }

                TextInput {
                    id: zoomTextInput

                    objectName: "zoomTextInput"

                    Accessible.editable: zoomSpinBox.editable
                    Accessible.focusable: zoomSpinBox.semanticInputAvailable
                    Accessible.focused: activeFocus
                    Accessible.ignored: !zoomSpinBox.semanticInputAvailable
                    Accessible.name: zoomSpinBox.formattedDisplayText.trim() + " %"
                    Accessible.readOnly: !zoomSpinBox.editable
                    Accessible.role: Accessible.SpinBox

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    activeFocusOnTab: zoomSpinBox.semanticInputAvailable
                    clip: true
                    color: zoomPresentationSurface.palette.text
                    enabled: zoomSpinBox.semanticInputAvailable
                    font: Kirigami.Theme.fixedWidthFont
                    horizontalAlignment: Text.AlignRight
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    readOnly: !zoomSpinBox.semanticInputAvailable
                    selectByMouse: true
                    selectedTextColor: zoomPresentationSurface.palette.highlightedText
                    selectionColor: zoomPresentationSurface.palette.highlight
                    verticalAlignment: Text.AlignVCenter

                    validator: IntValidator {
                        bottom: zoomSpinBox.from
                        top: zoomSpinBox.to
                    }

                    Accessible.onDecreaseAction: zoomSpinBox.decrease()
                    Accessible.onIncreaseAction: zoomSpinBox.increase()

                    Binding {
                        property: "text"
                        target: zoomTextInput
                        value: zoomSpinBox.formattedDisplayText
                        when: !root.interactionEnabled || !zoomTextInput.activeFocus
                    }

                    Keys.onDownPressed: event => {
                        zoomSpinBox.decrease();
                        event.accepted = true;
                    }
                    Keys.onUpPressed: event => {
                        zoomSpinBox.increase();
                        event.accepted = true;
                    }

                    onActiveFocusChanged: {
                        if (activeFocus && root.interactionEnabled && zoomSpinBox.enabled) {
                            text = zoomSpinBox.editableDisplayText;
                        }
                    }
                    onEditingFinished: zoomSpinBox.commitEditing(false)
                }

                Text {
                    id: zoomPercentSuffixLabel

                    objectName: "zoomPercentSuffixLabel"

                    readonly property int trailingSpacing: Layout.rightMargin

                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: root.controlSpacing

                    color: zoomPresentationSurface.palette.text
                    font: Kirigami.Theme.fixedWidthFont
                    text: "%"
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item {
                id: stepperColumn

                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                width: zoomPresentationSurface.stepperWidth

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.top: parent.top
                    color: zoomPresentationSurface.palette.mid
                    width: 1
                }

                Item {
                    id: increaseStepSurface

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: parent.height / 2

                    Rectangle {
                        anchors.fill: parent
                        color: increaseStepInput.pressed ? Kirigami.ColorUtils.linearInterpolation(zoomPresentationSurface.palette.base, zoomPresentationSurface.palette.highlight, 0.3) : (increaseStepInput.containsMouse ? Kirigami.ColorUtils.linearInterpolation(zoomPresentationSurface.palette.base, zoomPresentationSurface.palette.highlight, 0.18) : "transparent")
                    }

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        color: zoomPresentationSurface.palette.buttonText
                        enabled: zoomSpinBox.increasePresentationAvailable
                        height: Math.min(parent.height, parent.width) * 0.55
                        source: "go-up-symbolic"
                        width: height
                    }

                    MouseArea {
                        id: increaseStepInput

                        property bool repeated: false

                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        enabled: zoomSpinBox.semanticInputAvailable && zoomSpinBox.increasePresentationAvailable
                        hoverEnabled: true

                        onCanceled: increaseRepeatTimer.stop()
                        onClicked: {
                            if (!repeated) {
                                zoomSpinBox.increase();
                            }
                        }
                        onPressAndHold: {
                            repeated = true;
                            zoomSpinBox.increase();
                            increaseRepeatTimer.start();
                        }
                        onPressed: {
                            repeated = false;
                            zoomSpinBox.forceActiveFocus();
                        }
                        onReleased: increaseRepeatTimer.stop()

                        Timer {
                            id: increaseRepeatTimer

                            interval: 100
                            repeat: true

                            onTriggered: zoomSpinBox.increase()
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: zoomPresentationSurface.palette.mid
                    height: 1
                }

                Item {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: parent.height / 2

                    Rectangle {
                        anchors.fill: parent
                        color: decreaseStepInput.pressed ? Kirigami.ColorUtils.linearInterpolation(zoomPresentationSurface.palette.base, zoomPresentationSurface.palette.highlight, 0.3) : (decreaseStepInput.containsMouse ? Kirigami.ColorUtils.linearInterpolation(zoomPresentationSurface.palette.base, zoomPresentationSurface.palette.highlight, 0.18) : "transparent")
                    }

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        color: zoomPresentationSurface.palette.buttonText
                        enabled: zoomSpinBox.decreasePresentationAvailable
                        height: Math.min(parent.height, parent.width) * 0.55
                        source: "go-down-symbolic"
                        width: height
                    }

                    MouseArea {
                        id: decreaseStepInput

                        property bool repeated: false

                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        enabled: zoomSpinBox.semanticInputAvailable && zoomSpinBox.decreasePresentationAvailable
                        hoverEnabled: true

                        onCanceled: decreaseRepeatTimer.stop()
                        onClicked: {
                            if (!repeated) {
                                zoomSpinBox.decrease();
                            }
                        }
                        onPressAndHold: {
                            repeated = true;
                            zoomSpinBox.decrease();
                            decreaseRepeatTimer.start();
                        }
                        onPressed: {
                            repeated = false;
                            zoomSpinBox.forceActiveFocus();
                        }
                        onReleased: decreaseRepeatTimer.stop()

                        Timer {
                            id: decreaseRepeatTimer

                            interval: 100
                            repeat: true

                            onTriggered: zoomSpinBox.decrease()
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        context: Qt.WindowShortcut
        enabled: root.visible && root.textInputActive
        sequence: "Return"

        onActivated: root.commitEditing(true)
    }

    Shortcut {
        context: Qt.WindowShortcut
        enabled: root.visible && root.textInputActive
        sequence: "Enter"

        onActivated: root.commitEditing(true)
    }
}
