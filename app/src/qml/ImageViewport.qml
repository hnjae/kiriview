// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQml
import org.hnjae.kiriview

MediaViewportDelegate {
    id: root

    readonly property var imageDocument: root.documentSession.imageDocument
    property bool imageReady: root.presentationActive && root.documentSession.activeImageReady
    readonly property bool imageHorizontallyPannable: root.presentationActive && root.imageDocument.viewportHorizontallyPannable
    readonly property bool imagePannable: root.presentationActive && root.imageDocument.viewportPannable

    function panBy(deltaX, deltaY) {
        return root.imageDocument.requestViewportPanBy(deltaX, deltaY);
    }

    function nearestImageViewportPoint(viewportX, viewportY) {
        if (!root.imageReady) {
            return null;
        }

        const point = root.imageDocument.nearestImageViewportPoint(Qt.point(viewportX, viewportY));
        return Number.isFinite(point.x) && Number.isFinite(point.y) ? point : null;
    }

    function zoomByStep(stepCount, viewportX, viewportY) {
        return root.imageReady && root.imageDocument.requestZoomByStep(stepCount, Qt.point(viewportX, viewportY));
    }

    function toggleFitOrActualSize(viewportX, viewportY) {
        return root.imageReady && root.imageDocument.requestToggleFitOrActualSize(Qt.point(viewportX, viewportY));
    }

    function handleWheelZoom(wheel) {
        const stepCount = wheelZoomPolicy.stepCount(wheel);
        const anchorPoint = root.nearestImageViewportPoint(wheel.x, wheel.y);
        if (stepCount === 0 || anchorPoint === null) {
            wheel.accepted = false;
            return;
        }

        wheel.accepted = root.zoomByStep(stepCount, anchorPoint.x, anchorPoint.y);
    }

    ZoomWheelStepPolicy {
        id: wheelZoomPolicy
    }

    KiriImageViewportSurface {
        id: viewportSurface

        anchors.fill: parent
        document: root.presentationActive ? root.imageDocument : null
        objectName: "imageViewportSurface"
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.ControlModifier
        blocking: true
        enabled: root.imageReady
        target: null

        onWheel: wheel => root.handleWheelZoom(wheel)
    }

    WheelHandler {
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier
        blocking: true
        enabled: root.imageReady && root.contextMenuButtonPressed
        target: null

        onWheel: wheel => {
            if ((wheel.buttons & Qt.RightButton) === 0) {
                wheel.accepted = false;
                return;
            }

            root.markContextMenuTapSuppressed();
            root.handleWheelZoom(wheel);
        }
    }

    DragHandler {
        id: dragPanHandler

        property point previousTranslation: Qt.point(0, 0)

        acceptedButtons: Qt.LeftButton
        enabled: root.imageReady && root.imagePannable
        target: null

        onActiveChanged: {
            previousTranslation = Qt.point(0, 0);
        }
        onTranslationChanged: {
            if (!active) {
                previousTranslation = Qt.point(0, 0);
                return;
            }

            const panDelta = Qt.point(translation.x - previousTranslation.x, translation.y - previousTranslation.y);
            previousTranslation = translation;
            root.panBy(-panDelta.x, -panDelta.y);
        }
    }

    Controls.ScrollBar {
        id: horizontalScrollBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: verticalScrollBar.visible ? verticalScrollBar.left : parent.right
        orientation: Qt.Horizontal
        policy: root.imageHorizontallyPannable ? Controls.ScrollBar.AsNeeded : Controls.ScrollBar.AlwaysOff
        position: root.presentationActive ? root.imageDocument.horizontalScrollPosition : 0
        size: root.presentationActive ? root.imageDocument.horizontalScrollPageSize : 1

        onPositionChanged: {
            if (pressed) {
                root.imageDocument.submitHorizontalScrollPosition(position);
            }
        }
    }

    Controls.ScrollBar {
        id: verticalScrollBar

        anchors.bottom: horizontalScrollBar.visible ? horizontalScrollBar.top : parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        orientation: Qt.Vertical
        policy: root.presentationActive && root.imageDocument.viewportVerticallyPannable ? Controls.ScrollBar.AsNeeded : Controls.ScrollBar.AlwaysOff
        position: root.presentationActive ? root.imageDocument.verticalScrollPosition : 0
        size: root.presentationActive ? root.imageDocument.verticalScrollPageSize : 1

        onPositionChanged: {
            if (pressed) {
                root.imageDocument.submitVerticalScrollPosition(position);
            }
        }
    }

    Timer {
        id: singleClickTimer

        // qmllint disable missing-property
        interval: Qt.application.styleHints.mouseDoubleClickInterval
        // qmllint enable missing-property
        repeat: false

        onTriggered: root.viewerClicked()
    }

    TapHandler {
        id: clickHandler

        acceptedButtons: Qt.LeftButton

        onDoubleTapped: eventPoint => {
            singleClickTimer.stop();
            root.toggleFitOrActualSize(eventPoint.position.x, eventPoint.position.y);
        }
        onTapped: singleClickTimer.restart()
    }

    HoverHandler {
        cursorShape: !root.imagePannable ? Qt.ArrowCursor : dragPanHandler.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        enabled: root.imageReady
    }
}
