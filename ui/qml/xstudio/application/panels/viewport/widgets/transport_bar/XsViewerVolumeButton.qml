// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic


import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsPrimaryButton{ id: volumeButton
    imgSrc: muted? "qrc:/icons/volume_off.svg":
    volume==0? "qrc:/icons/volume_no_sound.svg":
    volume<=50? "qrc:/icons/volume_down.svg":
    "qrc:/icons/volume_up.svg"

    isActive: muted

    property color bgColorPressed: XsStyleSheet.accentColor
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: "#E6676767" //bgColorNormal

    property alias slider: volumeSlider

    onClicked: {
        muted = !muted
        if(muted && popup.visible)
            popup.close()
    }


    onHoveredChanged: {
        if(hovered){
            if(!popup.visible)
                popup.open()

            if(auto_hide_timer.running)
                auto_hide_timer.stop()
        } else if(!hovered && !auto_hide_timer.running && popup.visible) {
            auto_hide_timer.start()
        }
    }

    Timer {
        id: auto_hide_timer
        interval: 500
        running: false
        repeat: false
        onTriggered: popup.close()
    }

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: audio_output_model_data
        modelDataName: "audio_output"
    }
    XsAttributeValue {
        id: __volume
        attributeTitle: "volume"
        model: audio_output_model_data
    }
    property alias volume: __volume.value
    XsAttributeValue {
        id: __muted
        attributeTitle: "muted"
        model: audio_output_model_data
    }
    property alias muted: __muted.value

    // follow the backend volume. If it has changed and user isn't
    // interacting with the slider then we update the slider value to follow
    property var backendVolume: volume
    onBackendVolumeChanged: {
        if (!volumeSlider.pressed) {
            volumeSlider.value = backendVolume/10
        }
    }

    XsPopup { id: popup
        width: parent.width
        height: parent.width * 5
        x: 0
        y: -height

        closePolicy: Popup.CloseOnEscape

        HoverHandler {
            onHoveredChanged: {
                if(hovered && auto_hide_timer.running)
                    auto_hide_timer.stop()
                else if (!hovered)
                    auto_hide_timer.start()
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0


            XsText{ id: valueDisplay
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight+(2*2)
                Layout.preferredWidth: parent.width
                text: parseInt(volume/10 + 0.5)
                font.bold: true
            }

            XsSlider{ id: volumeSlider
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width
                snapMode: Slider.NoSnap
                stepSize: .1
                orientation: Qt.Vertical
                fillColor: muted? Qt.darker(XsStyleSheet.accentColor,2) : XsStyleSheet.accentColor
                handleColor: muted? Qt.darker(XsStyleSheet.primaryTextColor,1.2) : XsStyleSheet.primaryTextColor
                onValueChanged: {
                    if (pressed) {
                        volume = value*10.0
                        if (value) muted = false
                    }
                }
            }
        }
    }
}
