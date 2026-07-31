// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.hnjae.kiriview
import org.kde.ki18n
import org.kde.kirigami as Kirigami

Controls.Control {
    id: root

    objectName: "videoPlaybackControls"

    required property KiriVideoPlaybackControls playbackControls
    readonly property bool fixedMode: playbackControls.fixedMode
    readonly property bool interactionActive: controlsHoverHandler.hovered || playPauseButton.pressed || playPauseButton.activeFocus || timelineSlider.pressed || timelineSlider.activeFocus || muteButton.pressed || muteButton.activeFocus
    readonly property real floatingNaturalWidth: leftPadding + rightPadding + controlsRow.implicitWidth
    readonly property real floatingSideMargin: Kirigami.Units.largeSpacing
    readonly property real availableResponsiveWidth: parent ? Math.max(0, parent.width - floatingSideMargin * 2) : floatingNaturalWidth
    readonly property real preferredResponsiveWidth: parent ? parent.width * 0.75 : floatingNaturalWidth
    readonly property real floatingWidth: parent ? Math.min(availableResponsiveWidth, Math.max(floatingNaturalWidth, preferredResponsiveWidth)) : floatingNaturalWidth

    leftPadding: Kirigami.Units.smallSpacing
    rightPadding: Kirigami.Units.smallSpacing
    topPadding: Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2))
    bottomPadding: Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2))
    enabled: playbackControls.shown
    opacity: playbackControls.shown ? 1 : 0
    width: fixedMode && parent ? parent.width : floatingWidth

    onInteractionActiveChanged: playbackControls.reportInteractionActive(interactionActive)
    onVisibleChanged: {
        if (visible) {
            playbackControls.reveal();
        }
    }

    Behavior on opacity {
        enabled: !root.fixedMode

        NumberAnimation {
            duration: Kirigami.Units.shortDuration
            easing.type: Easing.InOutQuad
        }
    }

    Component.onCompleted: playbackControls.reportInteractionActive(interactionActive)
    Component.onDestruction: {
        playbackControls.cancelScrub();
        playbackControls.reportInteractionActive(false);
    }

    HoverHandler {
        id: controlsHoverHandler
    }

    background: Kirigami.ShadowedRectangle {
        color: Kirigami.Theme.backgroundColor
        opacity: root.fixedMode ? 0.96 : 0.84
        radius: root.fixedMode ? 0 : Kirigami.Units.cornerRadius

        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
        border.width: 1

        shadow.color: Qt.rgba(0, 0, 0, root.fixedMode ? 0 : 0.18)
        shadow.size: root.fixedMode ? 0 : Kirigami.Units.smallSpacing
        shadow.xOffset: 0
        shadow.yOffset: root.fixedMode ? 0 : Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2))
    }

    contentItem: RowLayout {
        id: controlsRow

        spacing: Kirigami.Units.smallSpacing

        Controls.ToolButton {
            id: playPauseButton

            objectName: "videoPlaybackPlayPauseButton"

            Accessible.name: text
            Accessible.role: Accessible.Button
            display: Controls.AbstractButton.IconOnly
            icon.name: root.playbackControls.playing ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
            text: root.playbackControls.playing ? KI18n.i18nc("@action:button", "Pause") : KI18n.i18nc("@action:button", "Play")

            Controls.ToolTip.text: text
            Controls.ToolTip.visible: hovered && Controls.ToolTip.text.length > 0 && !Kirigami.Settings.hasTransientTouchInput

            onPressedChanged: {
                if (pressed) {
                    root.playbackControls.reveal();
                }
            }
            onClicked: root.playbackControls.togglePlayback()
        }

        Controls.Label {
            id: currentTimeLabel

            objectName: "videoPlaybackCurrentTimeLabel"

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
            color: Kirigami.Theme.textColor
            elide: Text.ElideRight
            font: Kirigami.Theme.fixedWidthFont
            fontSizeMode: Text.HorizontalFit
            horizontalAlignment: Text.AlignRight
            maximumLineCount: 1
            minimumPixelSize: Math.max(8, Kirigami.Theme.smallFont.pixelSize - 3)
            text: root.playbackControls.currentTimeText
        }

        Controls.Slider {
            id: timelineSlider

            objectName: "videoPlaybackSlider"

            Accessible.name: KI18n.i18nc("@label:slider", "Position")
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 4
            enabled: root.playbackControls.timelineInteractive
            from: 0
            live: false
            stepSize: 1000
            to: root.playbackControls.sliderMaximumMsec
            value: root.playbackControls.sliderValueMsec

            onMoved: {
                if (pressed) {
                    root.playbackControls.updateScrub(Math.round(value));
                } else {
                    root.playbackControls.requestSeek(Math.round(value));
                }
            }
            onPressedChanged: {
                if (pressed) {
                    root.playbackControls.beginScrub();
                    return;
                }
                root.playbackControls.commitScrub();
            }

            Keys.priority: Keys.AfterItem
            Keys.onPressed: event => {
                if (event.modifiers !== Qt.NoModifier) {
                    return;
                }

                switch (event.key) {
                case Qt.Key_Left:
                case Qt.Key_Right:
                case Qt.Key_Up:
                case Qt.Key_Down:
                    event.accepted = true;
                    break;
                default:
                    break;
                }
            }
        }

        Controls.Label {
            id: durationTimeLabel

            objectName: "videoPlaybackDurationLabel"

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
            color: Kirigami.Theme.textColor
            elide: Text.ElideRight
            font: Kirigami.Theme.fixedWidthFont
            fontSizeMode: Text.HorizontalFit
            horizontalAlignment: Text.AlignLeft
            maximumLineCount: 1
            minimumPixelSize: Math.max(8, Kirigami.Theme.smallFont.pixelSize - 3)
            text: root.playbackControls.durationText
        }

        Controls.ToolButton {
            id: muteButton

            objectName: "videoPlaybackMuteButton"

            Accessible.name: text
            Accessible.role: Accessible.Button
            display: Controls.AbstractButton.IconOnly
            icon.name: root.playbackControls.muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic"
            text: root.playbackControls.muted ? KI18n.i18nc("@action:button", "Unmute") : KI18n.i18nc("@action:button", "Mute")

            Controls.ToolTip.text: text
            Controls.ToolTip.visible: hovered && Controls.ToolTip.text.length > 0 && !Kirigami.Settings.hasTransientTouchInput

            onPressedChanged: {
                if (pressed) {
                    root.playbackControls.reveal();
                }
            }
            onClicked: root.playbackControls.toggleMuted()
        }
    }
}
