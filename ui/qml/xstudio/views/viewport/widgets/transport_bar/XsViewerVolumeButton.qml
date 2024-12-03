// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.3

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsPrimaryButton{ id: volumeButton
    imgSrc: muted? "qrc:/icons/volume_mute.svg":
    volume==0? "qrc:/icons/volume_no_sound.svg":
    volume<=50? "qrc:/icons/volume_down.svg":
    "qrc:/icons/volume_up.svg"

    isActive: popup.visible

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: "#E6676767" //bgColorNormal

    // property alias btnIcon: muteButton.imgSrc
    property alias slider: volumeSlider

    onClicked:{
        popup.open()
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
        height: parent.width*5
        x: 0
        y: -height //+(width/1.25)

        ColumnLayout {
            anchors.fill: parent
            spacing: 0


            XsText{ id: valueDisplay
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight+(2*2)
                Layout.preferredWidth: parent.width
                text: volume/10
                // opacity: muted? 0.7:1
                font.bold: true
            }

            XsSlider{ id: volumeSlider
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width
                snapMode: Slider.NoSnap
                stepSize: .1
                orientation: Qt.Vertical
                fillColor: muted? Qt.darker(palette.highlight,2) : palette.highlight
                handleColor: muted? Qt.darker(palette.text,1.2) : palette.text
                onValueChanged: {
                    if (pressed) {
                        volume = parseInt(value)*10
                        if (value) muted = false
                    }
                }
                onReleased:{
                    popup.close()
                }
            }

            // Item{

            //     Layout.preferredHeight: XsStyleSheet.widgetStdHeight //+(2*2)
            //     Layout.preferredWidth: parent.width

            //     XsSecondaryButton{ id: muteButton
            //         anchors.centerIn: parent
            //         width: 20 //XsStyleSheet.secondaryButtonStdWidth
            //         height: 20
            //         imgSrc: "qrc:/icons/volume_mute.svg"
            //         isActive: muted != undefined ? muted : false
            //         showHoverOnActive: true
            //         property color bgColorPressed: palette.highlight
            //         property color bgColorNormal: "#1AFFFFFF"
            //         property color forcedBgColorNormal: "#E6676767" //bgColorNormal

            //         onClicked:{
            //             muted = !muted
            //             popup.close()
            //         }
            //     }

            // }


        }

    }

}
