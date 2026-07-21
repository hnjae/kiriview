// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick

FocusScope {
    id: root

    required property var documentSession
    property bool presentationActive: true
    property bool contextMenuButtonPressed: false
    property bool suppressNextContextMenuTap: false
    signal viewerClicked
    signal viewerContextMenuRequested(var popupParent, point position)

    function markContextMenuTapSuppressed() {
        root.suppressNextContextMenuTap = true;
    }

    function requestViewportFocus() {
        root.forceActiveFocus();
    }

    activeFocusOnTab: true
    focus: true

    TapHandler {
        id: contextMenuTapHandler

        acceptedButtons: Qt.RightButton
        enabled: root.presentationActive

        onPressedChanged: {
            root.contextMenuButtonPressed = contextMenuTapHandler.pressed;
            if (contextMenuTapHandler.pressed) {
                root.suppressNextContextMenuTap = false;
            }
        }
        onTapped: {
            if (root.suppressNextContextMenuTap) {
                root.suppressNextContextMenuTap = false;
                return;
            }

            root.viewerContextMenuRequested(root, contextMenuTapHandler.point.position);
        }
    }
}
