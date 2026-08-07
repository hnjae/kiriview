// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs as Dialogs
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.statefulapp as StatefulApp

StatefulApp.StatefulWindow {
    id: root

    required property KiriDocumentSession documentSession
    required property KiriViewApplication kiriApplication
    required property KiriWindowShell windowShell

    application: kiriApplication
    title: windowShell.windowTitle
    visible: true
    windowName: "Main"

    property bool helpDialogOpen: false
    readonly property bool fullscreen: windowShell.fullscreen
    readonly property bool menuBarMode: kiriApplication.menuPresentation === KiriViewApplication.MenuBar
    readonly property bool applicationMenuShortcutEnabled: !root.menuBarMode && !root.fullscreen && !root.helpDialogOpen

    function canOpenDroppedUrls(dropEvent) {
        return dropEvent.hasUrls && dropEvent.urls.length > 0;
    }

    function openDroppedUrls(urls) {
        if (urls.length <= 0) {
            return;
        }

        documentSession.sourceUrl = urls[0];
    }

    function activeImageToolBar() {
        return mainImageToolBar;
    }

    function activeMenuHost() {
        return root.activeImageToolBar();
    }

    function focusActiveViewport() {
        mediaWorkspaceHost.forceActiveViewportFocus();
    }

    function openApplicationMenu() {
        return root.activeMenuHost().openApplicationMenu();
    }

    function toolbarTextInputFocused() {
        return activeImageToolBar().textInputFocused();
    }

    function publishActionUiState() {
        kiriApplication.updateActionUiGateSnapshot(root.helpDialogOpen, root.toolbarTextInputFocused(), mediaWorkspaceHost.infoPanelVisible, mediaWorkspaceHost.thumbnailPanelVisible, root.fullscreen, root.applicationMenuShortcutEnabled, !root.helpDialogOpen);
    }

    minimumWidth: Kirigami.Units.gridUnit * 14
    minimumHeight: Kirigami.Units.gridUnit * 12
    width: Kirigami.Units.gridUnit * 24
    height: Kirigami.Units.gridUnit * 20

    onHelpDialogOpenChanged: windowShell.reportHelpDialogOpen(helpDialogOpen)

    Component.onCompleted: {
        windowShell.reportHelpDialogOpen(root.helpDialogOpen);
        root.publishActionUiState();
    }

    Connections {
        target: root.kiriApplication

        function onCancelToolbarTextInputEditingRequested() {
            root.activeImageToolBar().cancelTextInputEditing(true);
        }

        function onCloseInfoPanelRequested() {
            mediaWorkspaceHost.closeInfoPanel();
        }

        function onOpenApplicationMenuRequested() {
            root.openApplicationMenu();
        }

        function onOpenDialogRequested() {
            fileDialog.open();
        }

        function onShortcutHelpRequested() {
            shortcutHelpDialog.open();
        }

        function onToggleInfoPanelRequested() {
            mediaWorkspaceHost.toggleInfoPanel();
        }

        function onToggleThumbnailPanelRequested() {
            mediaWorkspaceHost.toggleThumbnailPanel();
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: !root.helpDialogOpen
        z: 100

        onDropped: drop => {
            if (!root.canOpenDroppedUrls(drop)) {
                drop.accepted = false;
                return;
            }

            root.openDroppedUrls(drop.urls);
            drop.acceptProposedAction();
        }
        onEntered: drag => {
            drag.accepted = root.canOpenDroppedUrls(drag);
        }
        onPositionChanged: drag => {
            drag.accepted = root.canOpenDroppedUrls(drag);
        }
    }

    pageStack.initialPage: Kirigami.Page {
        id: page

        readonly property var imageDocument: root.documentSession.imageDocument
        readonly property var videoDocument: root.documentSession.videoDocument
        readonly property bool imageMode: root.documentSession.documentKind === KiriDocumentSession.Image
        readonly property bool videoMode: root.documentSession.documentKind === KiriDocumentSession.Video
        readonly property bool imageReady: root.documentSession.activeImageReady
        readonly property string actionUiGateFingerprint: [root.helpDialogOpen, root.fullscreen, root.applicationMenuShortcutEnabled, root.toolbarTextInputFocused(), mediaWorkspaceHost.infoPanelVisible, mediaWorkspaceHost.thumbnailPanelVisible].join("|")

        background: Rectangle {
            color: imageViewTheme.darkBackgroundColor
        }

        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        Item {
            id: imageViewTheme

            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false

            readonly property color viewBackgroundColor: Kirigami.Theme.backgroundColor
            readonly property color viewTextColor: Kirigami.Theme.textColor
            readonly property bool lightColorScheme: viewBackgroundColor.hslLightness > viewTextColor.hslLightness
            readonly property color darkBackgroundColor: lightColorScheme ? viewTextColor : viewBackgroundColor
            readonly property color darkForegroundColor: lightColorScheme ? viewBackgroundColor : viewTextColor
        }

        MediaWorkspaceHost {
            id: mediaWorkspaceHost

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: root.fullscreen ? parent.top : mainImageToolBar.bottom

            documentSession: root.documentSession
            openAction: imageActions.openAction
            viewerForegroundColor: imageViewTheme.darkForegroundColor
            viewerSurfaceColor: imageViewTheme.darkBackgroundColor

            onViewerClicked: {
                if (root.activeImageToolBar().commitTextInputEditing(true)) {
                    return;
                }

                root.focusActiveViewport();
            }
            onViewerContextMenuRequested: function (popupParent, position) {
                root.activeImageToolBar().commitTextInputEditing(true);
                root.focusActiveViewport();
                viewerContextMenu.popup(popupParent, position.x, position.y);
            }
        }

        Item {
            id: fullscreenPointerTrackingArea

            anchors.fill: parent
            enabled: root.fullscreen
            visible: enabled
            z: 999

            HoverHandler {
                id: fullscreenPointerTrackingHoverHandler

                cursorShape: root.windowShell.pointerHidden ? Qt.BlankCursor : Qt.ArrowCursor
                enabled: fullscreenPointerTrackingArea.enabled

                onPointChanged: root.windowShell.reportPointerMoved(point.position.y >= 0 && point.position.y <= fullscreenToolBarRevealArea.height)
            }
        }

        ImageActions {
            id: imageActions

            application: root.kiriApplication
            documentSession: root.documentSession
            imageDocument: page.imageDocument
            videoMode: page.videoMode
        }

        onActionUiGateFingerprintChanged: root.publishActionUiState()

        ToastNotification {
            id: toastNotification

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            notificationText: root.windowShell.notificationMessage
            notificationVisible: root.windowShell.notificationActive
            replayRevision: root.windowShell.notificationReplayRevision
            z: 999

            onDismissed: root.windowShell.dismissNotification()
        }

        ContextActionMenu {
            id: viewerContextMenu

            objectName: "viewerContextMenu"
            actions: imageActions.contextMenuActions
        }

        Item {
            id: fullscreenToolBarRevealArea

            readonly property bool hovered: fullscreenToolBarRevealHoverHandler.hovered

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            enabled: root.fullscreen && !root.helpDialogOpen
            height: mainImageToolBar.implicitHeight + Kirigami.Units.largeSpacing
            visible: enabled
            z: 19

            HoverHandler {
                id: fullscreenToolBarRevealHoverHandler

                enabled: fullscreenToolBarRevealArea.enabled

                onHoveredChanged: {
                    if (hovered) {
                        root.windowShell.reportTopRevealEntered();
                    }
                }
            }
        }

        ImageDocumentToolBar {
            id: mainImageToolBar

            objectName: "mainImageToolBar"

            actions: imageActions
            activeNavigationAvailable: root.documentSession.activeNavigationAvailable
            activeNavigationCount: root.documentSession.activeNavigationCount
            activeNavigationCurrentNumber: root.documentSession.activeNavigationCurrentNumber
            activeNavigationEditable: root.documentSession.activeNavigationEditable
            activeNavigationKnown: root.documentSession.activeNavigationKnown
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            enabled: visible
            height: implicitHeight
            imageDocument: page.imageDocument
            imageReady: page.imageReady
            navigationPresentationProvider: root.kiriApplication
            applicationMenuActions: imageActions.applicationMenuActions
            openActiveNavigationAtNumber: function (number) {
                root.documentSession.openActiveNavigationAtNumber(number);
            }
            showApplicationMenuActions: !root.menuBarMode && !root.fullscreen
            transientOverlay: root.fullscreen
            visible: !root.fullscreen || root.windowShell.toolbarRevealed
            zoomEditable: root.documentSession.activeZoomEditable
            zoomPercent: root.documentSession.activeZoomPercent
            zoomPercentAvailable: root.documentSession.activeZoomPercentAvailable
            zoomPercentKnown: root.documentSession.activeZoomPercentKnown
            z: 20

            onTextInputFocusReturnRequested: root.focusActiveViewport()

            onInteractionActiveChanged: {
                root.windowShell.reportToolbarInteractionActive(interactionActive);
            }
        }
    }

    menuBar: ApplicationMenuBar {
        actions: imageActions
        collectionMode: root.documentSession.activeImageOpenedCollectionScopeActive
        imageMode: page.imageMode
        mediaMode: page.imageMode || page.videoMode
        navigationPresentationProvider: root.kiriApplication
        visible: root.menuBarMode && !root.fullscreen
    }

    ShortcutHelpDialog {
        id: shortcutHelpDialog

        application: root.kiriApplication

        onClosed: root.helpDialogOpen = false
        onOpened: root.helpDialogOpen = true
    }

    Dialogs.FileDialog {
        id: fileDialog

        objectName: "openFileDialog"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: root.documentSession.openDialogNameFilters
        title: KI18n.i18nc("@title:window", "Open Image, Video, or Comic Book")

        onAccepted: root.documentSession.sourceUrl = selectedFile
    }
}
