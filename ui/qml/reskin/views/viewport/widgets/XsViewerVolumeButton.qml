// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.3

import xStudioReskin 1.0

XsPrimaryButton{ id: volumeButton
    imgSrc: isMute? "qrc:/icons/volume_mute.svg":
    volume==0? "qrc:/icons/volume_no_sound.svg":
    volume<=5? "qrc:/icons/volume_down.svg":
    "qrc:/icons/volume_up.svg"

    isActive: popup.visible

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: "#E6676767" //bgColorNormal

    property alias volume: volumeSlider.value
    property alias btnIcon: muteButton.imgSrc
    property bool isMute: false

    onClicked:{
        popup.open()
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
                text: parseInt(volume)
                // opacity: isMute? 0.7:1
                font.bold: true
            }

            XsSlider{ id: volumeSlider
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width
                orientation: Qt.Vertical
                fillColor: isMute? Qt.darker(palette.highlight,2) : palette.highlight
                handleColor: isMute? Qt.darker(palette.text,1.2) : palette.text
                onValueChanged: isMute = false

                onReleased:{
                    popup.close()
                }
            }
            Item{
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight //+(2*2)
                Layout.preferredWidth: parent.width

                XsSecondaryButton{ id: muteButton
                    anchors.centerIn: parent
                    width: 20 //XsStyleSheet.secondaryButtonStdWidth
                    height: 20
                    imgSrc: "qrc:/icons/volume_mute.svg"
                    isActive: isMute
                    property color bgColorPressed: palette.highlight
                    property color bgColorNormal: "#1AFFFFFF"
                    property color forcedBgColorNormal: "#E6676767" //bgColorNormal
                    
                
                    onClicked:{
                        isMute = !isMute
                        popup.close()
                    }
                }

            }

            
        }

    }
    
}
