// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

QtObject {
    id: root

    required property var rightToLeftSourceAction
    required property var twoPageSourceAction
    required property bool imageReady
    required property bool videoMode
    required property var retainPresentation
    required property bool fitEnabled
    required property int fitModeSelection
    required property bool zoomEditable
    required property bool zoomPercentAvailable
    required property bool zoomPercentKnown
    required property real zoomPercent

    readonly property bool presentationRetained: shouldRetainPresentation()
    readonly property int presentedFitModeSelection: shouldRetainPresentation() ? retainedFitModeSelection : fitModeSelection
    readonly property bool presentedImageReady: shouldRetainPresentation() || imageReady
    readonly property bool presentedZoomEditable: shouldRetainPresentation() ? retainedZoomEditable : zoomEditable
    readonly property bool presentedZoomPercentAvailable: shouldRetainPresentation() ? retainedZoomPercentAvailable : zoomPercentAvailable
    readonly property bool presentedZoomPercentKnown: shouldRetainPresentation() ? retainedZoomPercentKnown : zoomPercentKnown
    readonly property real presentedZoomPercent: shouldRetainPresentation() ? retainedZoomPercent : zoomPercent
    readonly property RetainedPresentationAction rightToLeftAction: RetainedPresentationAction {
        retainPresentation: root.retainPresentation
        sourceAction: root.rightToLeftSourceAction
        videoMode: root.videoMode
    }
    readonly property RetainedPresentationAction twoPageAction: RetainedPresentationAction {
        retainPresentation: root.retainPresentation
        sourceAction: root.twoPageSourceAction
        videoMode: root.videoMode
    }

    property bool retainedFitEnabled: false
    property int retainedFitModeSelection: 0
    property bool retainedZoomActionEnabled: false
    property bool retainedZoomEditable: false
    property bool retainedZoomPercentAvailable: false
    property bool retainedZoomPercentKnown: false
    property real retainedZoomPercent: 0

    component RetainedPresentationAction: Kirigami.Action {
        required property var sourceAction
        required property var retainPresentation
        required property bool videoMode
        property bool retainedChecked: false
        property bool retainedEnabled: false

        function shouldRetainPresentation() {
            return !videoMode && typeof retainPresentation === "function" && retainPresentation();
        }

        function capturePresentation() {
            retainedChecked = sourceAction?.checked ?? false;
            retainedEnabled = sourceAction?.enabled ?? false;
        }

        autoExclusive: sourceAction?.autoExclusive ?? false
        checkable: sourceAction?.checkable ?? false
        checked: shouldRetainPresentation() ? retainedChecked : (sourceAction?.checked ?? false)
        displayHint: sourceAction?.displayHint ?? Kirigami.DisplayHint.KeepVisible
        enabled: shouldRetainPresentation() ? retainedEnabled : (sourceAction?.enabled ?? false)
        icon.name: sourceAction?.icon.name ?? ""
        shortcut: ""
        text: sourceAction?.text ?? ""
        tooltip: sourceAction?.tooltip ?? text
        visible: sourceAction?.visible ?? true

        onTriggered: {
            if (!shouldRetainPresentation() && (sourceAction?.enabled ?? false)) {
                sourceAction.trigger();
            }
        }
    }

    function shouldRetainPresentation() {
        return !videoMode && typeof retainPresentation === "function" && retainPresentation();
    }

    function capture() {
        if (!imageReady || videoMode || shouldRetainPresentation()) {
            return;
        }

        rightToLeftAction.capturePresentation();
        twoPageAction.capturePresentation();
        retainedFitEnabled = fitEnabled;
        retainedFitModeSelection = fitModeSelection;
        retainedZoomActionEnabled = !videoMode && imageReady;
        retainedZoomEditable = zoomEditable;
        retainedZoomPercentAvailable = zoomPercentAvailable;
        retainedZoomPercentKnown = zoomPercentKnown;
        retainedZoomPercent = zoomPercent;
    }

    function scheduleCapture() {
        captureTimer.restart();
    }

    onFitEnabledChanged: scheduleCapture()
    onFitModeSelectionChanged: scheduleCapture()
    onImageReadyChanged: scheduleCapture()
    onPresentationRetainedChanged: scheduleCapture()
    onRightToLeftSourceActionChanged: scheduleCapture()
    onTwoPageSourceActionChanged: scheduleCapture()
    onVideoModeChanged: scheduleCapture()
    onZoomEditableChanged: scheduleCapture()
    onZoomPercentAvailableChanged: scheduleCapture()
    onZoomPercentChanged: scheduleCapture()
    onZoomPercentKnownChanged: scheduleCapture()

    Component.onCompleted: scheduleCapture()

    property Timer captureTimer: Timer {
        interval: 0
        onTriggered: root.capture()
    }

    property Connections rightToLeftSourceConnections: Connections {
        target: root.rightToLeftSourceAction

        function onCheckedChanged() {
            root.scheduleCapture();
        }

        function onEnabledChanged() {
            root.scheduleCapture();
        }
    }

    property Connections twoPageSourceConnections: Connections {
        target: root.twoPageSourceAction

        function onCheckedChanged() {
            root.scheduleCapture();
        }

        function onEnabledChanged() {
            root.scheduleCapture();
        }
    }
}
