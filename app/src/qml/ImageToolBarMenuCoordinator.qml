// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick

QtObject {
    id: root

    required property Item hostToolbar
    required property Item actionSurface
    required property Item anchorItem
    required property var menuPopup
    required property var menuActions
    required property bool menuEnabled

    property Item buttonAnchor: null
    property double closedTimestamp: 0
    readonly property bool open: menuPopup.visible || menuPopup.opened

    function buttonUsable(button) {
        if (!button || !button.visible || button.width <= 0 || button.height <= 0 || !actionSurface.visible) {
            return false;
        }

        try {
            const center = button.mapToItem(actionSurface, button.width / 2, button.height / 2);
            return center.x >= 0 && center.x <= actionSurface.width && center.y >= 0 && center.y <= actionSurface.height;
        } catch (error) {
            console.warn("KiriView ImageToolBar application menu button mapping failed");
            return false;
        }
    }

    function updateButtonAnchor(button) {
        if (buttonUsable(button)) {
            if (buttonAnchor === button || !buttonUsable(buttonAnchor)) {
                buttonAnchor = button;
            }
        } else if (buttonAnchor === button) {
            buttonAnchor = null;
        }
    }

    function popupMenu() {
        if (!menuEnabled || menuActions.length <= 0) {
            return false;
        }

        if (buttonUsable(buttonAnchor)) {
            const popupPosition = buttonAnchor.mapToItem(hostToolbar, 0, buttonAnchor.height);
            anchorItem.x = popupPosition.x;
            anchorItem.y = popupPosition.y;
            menuPopup.popup(anchorItem, 0, 0);
            return true;
        }

        menuPopup.popup(actionSurface, Math.max(0, actionSurface.width - menuPopup.implicitWidth), actionSurface.height);
        return true;
    }

    function openMenu() {
        if (open) {
            return true;
        }

        return popupMenu();
    }

    function toggleMenu() {
        if (open) {
            menuPopup.dismiss();
            return true;
        }

        return popupMenu();
    }

    function recordClosed() {
        closedTimestamp = Date.now();
    }
}
