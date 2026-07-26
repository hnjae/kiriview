// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQml

QtObject {
    id: root

    property bool pageNavigationFocused: false
    property bool zoomFocused: false
    readonly property bool active: pageNavigationFocused || zoomFocused

    signal pageNumberResetRequested
    signal cancelRequested(bool returnViewerFocus)
    signal commitRequested(bool returnViewerFocus)
    signal focusReturnRequested

    function cancel(returnViewerFocus) {
        if (!active) {
            return false;
        }

        cancelRequested(returnViewerFocus === undefined ? true : returnViewerFocus);
        return true;
    }

    function commit(returnViewerFocus) {
        if (!active) {
            return false;
        }

        commitRequested(returnViewerFocus === undefined ? true : returnViewerFocus);
        return true;
    }

    function resetPageNumber() {
        pageNumberResetRequested();
    }
}
