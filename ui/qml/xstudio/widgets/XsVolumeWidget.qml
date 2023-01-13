// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQml 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

import xstudio.qml.properties 1.0
import xstudio.qml.module 1.0

XsTrayButton {
    id: volume_button

	text: "Volume"
	icon.source: { volume == 0  ||  muted ? "qrc:///feather_icons/volume-x.svg" : volume < 50 ? "qrc:///feather_icons/volume-1.svg" : "qrc:///feather_icons/volume-2.svg" }
    tooltip: "Set output volume."

    prototype: false
    property bool ready: false
    property var volume: audio_attrs.volume ? audio_attrs.volume : 0
    property var muted: audio_attrs.muted === undefined ? false : audio_attrs.muted

    XsModuleAttributes {
        id: audio_attrs
        attributesGroupName: "audio_output"
        onAttrAdded: {
            if (attr_name == "volume") {
                audio_attrs.volume = volume_pref.properties.value * 1.0
            }
            else if (attr_name == "muted") {
                audio_attrs.muted = mute_pref.properties.value
            }
        }
    }

    XsPreferenceSet {
        id: volume_pref
        preferencePath: "/ui/qml/volume"
    }

    XsPreferenceSet {
        id: mute_pref
        preferencePath: "/ui/qml/audio_mute"
    }

    Timer {
        id: update_prefs_timer
        interval: 80
        repeat: false
        onTriggered: {
            volume_pref.properties.value = volume
            mute_pref.properties.value = muted
        }
    }

    onVolumeChanged: {
        update_prefs_timer.start()

        if (!slider.pressed) {
            popupwidget.value = volume
        }
    }

    onMutedChanged:{
        update_prefs_timer.start()
    }

    onClicked: {
        popupwidget.toggleShow()
    }

    XsPopup {
        id: popupwidget
        property alias value: slider.value
        y: -166

        ColumnLayout {
            // spacing: 10
            XsIntSlider {
                // enabled: !muted
                opacity: muted ? 0.25 : 1.0
                Layout.fillHeight: true
                Layout.preferredHeight: 150
                Layout.preferredWidth: 20
                Layout.alignment: Qt.AlignCenter
                id: slider
                factor: 10
                orientation: Qt.Vertical
                onUnPressed: popupwidget.close()
                onValueChanged: {
                    if (audio_attrs.muted !== undefined) audio_attrs.muted = false

                    // the+/- hack ensures a float is passed back to C++ as 'value' is int but 'volume'
                    // is a float and we otherwise get a type error. QML/JS doesn't do type casting
                    // apparently!
                    let seekValue = (value+0.1)-0.1
                    audio_attrs.volume = Math.round(seekValue * 10) / 10 //seekValue.toFixed(1)
                }
            }

            Rectangle {
                id: muteButton
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                Layout.preferredWidth: 26
                Layout.alignment: Qt.AlignCenter
                radius: 3

                color: buttonMouseArea.containsMouse ? XsStyle.highlightColor : "transparent"

                XsColoredImage {
                    anchors.fill:parent
                    anchors.margins: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter

                    source: { volume == 0  ||  muted ? "qrc:///feather_icons/volume-x.svg" : volume < 50 ? "qrc:///feather_icons/volume-1.svg" : "qrc:///feather_icons/volume-2.svg" }
    

                    MouseArea {
                        id: buttonMouseArea
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                        onClicked: {
                            audio_attrs.muted = !audio_attrs.muted
                            popupwidget.close()
                        }
                    }
                }
            }
        }
    }
}