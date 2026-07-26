// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property KiriImageDocument imageDocument
    required property bool imageReady
    required property var openAction
    required property bool unsupportedOpenedCollectionVideo

    readonly property bool imageLoading: imageDocument.status === KiriImageDocument.Loading
    readonly property bool loadingFeedbackVisible: imageLoading && loadingFeedbackArmed
    readonly property string loadingTargetKey: imageDocument.loadingTargetToken + "|" + imageDocument.twoPageModeEnabled
    readonly property bool retainedPresentationPending: imageLoading && imageDocument.displayedUrl.toString().length > 0
    readonly property bool replacementGraceActive: retainedPresentationPending && !loadingFeedbackVisible
    property bool loadingFeedbackArmed: false
    property string scheduledLoadingTargetKey: ""

    function cancelLoadingFeedback() {
        loadingFeedbackTimer.stop();
        loadingFeedbackArmed = false;
        scheduledLoadingTargetKey = "";
    }

    function scheduleLoadingFeedback() {
        if (!imageLoading) {
            cancelLoadingFeedback();
            return;
        }
        if (loadingFeedbackVisible) {
            return;
        }
        loadingFeedbackArmed = false;
        scheduledLoadingTargetKey = loadingTargetKey;
        loadingFeedbackTimer.restart();
    }

    onImageLoadingChanged: {
        if (imageLoading) {
            scheduleLoadingFeedback();
        } else {
            cancelLoadingFeedback();
        }
    }
    onLoadingTargetKeyChanged: {
        if (imageLoading && !loadingFeedbackVisible) {
            scheduleLoadingFeedback();
        }
    }
    Component.onCompleted: {
        if (imageLoading) {
            scheduleLoadingFeedback();
        }
    }

    Timer {
        id: loadingFeedbackTimer

        interval: 150
        onTriggered: {
            if (root.imageLoading && root.scheduledLoadingTargetKey === root.loadingTargetKey) {
                root.loadingFeedbackArmed = true;
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        visible: root.loadingFeedbackVisible

        Kirigami.LoadingPlaceholder {
            anchors.centerIn: parent
            width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 18)
        }
    }

    Kirigami.InlineMessage {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Kirigami.Units.largeSpacing
        anchors.right: parent.right
        text: root.imageDocument.errorString
        type: Kirigami.MessageType.Error
        visible: root.imageReady && !root.imageDocument.loading && root.imageDocument.errorString.length > 0
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        icon.name: "video-x-generic-symbolic"
        text: KI18n.i18nc("@info:placeholder", "KiriView can’t play this video from the selected collection.")
        visible: root.unsupportedOpenedCollectionVideo
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 24)
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        helpfulAction: root.openAction
        icon.name: "image-x-generic-symbolic"
        text: KI18n.i18nc("@info:placeholder", "No image selected")
        visible: root.imageDocument.status === KiriImageDocument.Null && !root.unsupportedOpenedCollectionVideo
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 18)
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        explanation: root.imageDocument.errorString
        helpfulAction: root.openAction
        icon.name: "dialog-error-symbolic"
        text: KI18n.i18nc("@info:placeholder", "Unable to open image")
        visible: root.imageDocument.status === KiriImageDocument.Error && !root.unsupportedOpenedCollectionVideo
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 24)
    }
}
