// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property string notificationText
    required property bool notificationVisible
    required property int replayRevision

    signal dismissed

    Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
    Kirigami.Theme.inherit: false

    implicitWidth: notification.implicitWidth
    implicitHeight: notification.implicitHeight + Kirigami.Units.largeSpacing
    width: implicitWidth
    height: implicitHeight
    visible: notificationVisible

    onReplayRevisionChanged: {
        if (!notificationVisible) {
            return;
        }

        notification.opacity = 0;
        notification.scale = 0.96;
        entranceTranslate.y = Kirigami.Units.smallSpacing;
        entranceAnimation.restart();
    }

    ParallelAnimation {
        id: entranceAnimation

        NumberAnimation {
            target: notification
            property: "opacity"
            from: 0
            to: 1
            duration: Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: notification
            property: "scale"
            from: 0.96
            to: 1
            duration: Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: entranceTranslate
            property: "y"
            from: Kirigami.Units.smallSpacing
            to: 0
            duration: Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }

    Controls.Control {
        id: notification

        anchors.bottom: parent.bottom
        anchors.bottomMargin: Kirigami.Units.largeSpacing
        anchors.horizontalCenter: parent.horizontalCenter
        leftPadding: Kirigami.Units.largeSpacing
        rightPadding: Kirigami.Units.largeSpacing
        topPadding: Kirigami.Units.largeSpacing
        bottomPadding: Kirigami.Units.largeSpacing
        implicitWidth: Math.min(implicitContentWidth + leftPadding + rightPadding, Math.min(Kirigami.Units.gridUnit * 25, root.parent ? root.parent.width / 1.5 : Kirigami.Units.gridUnit * 25))
        opacity: 1
        scale: 1
        transform: Translate {
            id: entranceTranslate
        }
        transformOrigin: Item.Center

        contentItem: Controls.Label {
            color: Kirigami.Theme.textColor
            elide: Text.ElideRight
            maximumLineCount: 3
            text: root.notificationText
            wrapMode: Text.Wrap
        }

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            opacity: 0.9
            radius: Kirigami.Units.cornerRadius
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton

            onTapped: root.dismissed()
        }
    }
}
