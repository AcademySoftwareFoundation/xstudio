import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtMultimedia

import xStudio 1.0

MouseArea {
    id: playerbox
    hoverEnabled: true

    onEntered: forceActiveFocus(playerbox)

    function loadMedia(path) {
        player.source = path
    }

    signal eject()

    onVisibleChanged: {
        if(visible)
            forceActiveFocus(playerbox)
    }

    onClicked: {
        if(player.playing)
            player.pause()
        else
            player.play()
    }

    function playToggle() {
        if(player.playing) {
            player.pause()
        } else {
            player.play()
        }
    }

    function exitPlayback() {
        player.stop()
        player.source = ""
        eject()
    }

    function audioToggle() {
        player.audioOutput.muted = !player.audioOutput.muted
    }

    Keys.onEscapePressed: playerbox.exitPlayback()
    Keys.onSpacePressed: playerbox.playToggle()

    Rectangle {
        anchors.fill: parent
        color: "black"

        VideoOutput {
            id: output
            anchors.fill: parent
        }

        MediaPlayer {
            id: player
            property real rate: 1000.0 / 24.0
            loops: MediaPlayer.Infinite
            videoOutput: output
            audioOutput: AudioOutput {
                volume: 0.5
            }
            onMetaDataChanged: {
                player.metaData.keys().forEach(function (item, index) {
                    if(player.metaData.metaDataKeyToString(item) == "Video frame rate") {
                        rate = 1000.0 / player.metaData.stringValue(item)
                    }
                })
            }
        }


        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 4
            height: parent.height / 20
            color: "#AA000000"

            RowLayout {
                spacing:4
                anchors.fill: parent

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    hoverEnabled: true

                    onClicked: playerbox.exitPlayback()

                    XsIcon {
                        anchors.fill: parent
                        source: "qrc:/icons/close.svg"
                        imgOverlayColor: parent.containsMouse ? palette.highlight : XsStyleSheet.hintColor
                    }
                }

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    hoverEnabled: true

                    onClicked: playerbox.playToggle()
                    onEntered: forceActiveFocus(playerbox)

                    XsIcon {
                        anchors.fill: parent
                        source: player.playing ? "qrc:/icons/play_arrow.svg" : "qrc:/icons/pause.svg"
                        imgOverlayColor: parent.containsMouse ? palette.highlight : XsStyleSheet.hintColor
                    }
                }

                ScrollBar {
                    id: scroller
                    hoverEnabled: true
                    enabled: player.seekable
                    Layout.preferredHeight: parent.height/3
                    Layout.fillWidth: true
                    orientation: Qt.Horizontal
                    policy: ScrollBar.AlwaysOn
                    position: player.duration ? Math.min(1.0,Math.max(0.0, (1.0/player.duration) * player.position))  : 0

                    size: player.duration ? (1.0/(player.duration/player.rate)): 1.0

                    onPositionChanged: {
                        if(pressed)
                            player.position = position * player.duration
                    }

                    contentItem: Rectangle {
                        height: scroller.height
                        radius: height / 2
                        color: scroller.pressed ? palette.highlight : (scroller.hovered ? palette.text : XsStyleSheet.hintColor)
                    }

                    background: Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: XsStyleSheet.widgetBgNormalColor
                    }

                    Keys.onEscapePressed: playerbox.exitPlayback()
                    Keys.onSpacePressed: playerbox.playToggle()
                }

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    enabled: player.hasAudio
                    hoverEnabled: true

                    onClicked: playerbox.audioToggle()

                    XsIcon {
                        anchors.fill: parent
                        source: "qrc:/icons/volume_off.svg"
                        imgOverlayColor: player.hasAudio ? (parent.containsMouse ? palette.highlight : (player.audioOutput.muted ? palette.highlight : XsStyleSheet.hintColor)) : XsStyleSheet.widgetBgNormalColor
                    }
                }

            }
        }
    }
}