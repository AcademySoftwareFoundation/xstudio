// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.helpers 1.0

XsWindow{
    id: popup

    title: ""
    property string body: ""
    property string initialText: ""
    property var choices: []
    width: 400
    height: 300
    signal response(variant text, variant fps, variant button_press)
    property var chaser


    XsPreference {
        id: sessionRate
        path: "/core/session/media_rate"
    }

    ColumnLayout {

        id: layout
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        XsText {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            text: body
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.weight: Font.Bold
            font.pixelSize: XsStyleSheet.fontSize*1.2
        }

        GridLayout {

            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 6

            XsLabel {
                Layout.alignment: Qt.AlignRight
                text: "Name"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
            }

            Rectangle {

                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "transparent"
                border.color: "black"

                Keys.onReturnPressed:{
                    popup.response(input.text, rateCB.currentText, popup.choices[popup.choices.length-1])
                    popup.visible = false
                }
                Keys.onEscapePressed: {
                    popup.response(input.text, rateCB.currentText, popup.choices[0])
                    popup.visible = false
                }

                XsTextField{ 
                    
                    id: input
                    anchors.fill: parent
                    text: initialText
                    wrapMode: Text.Wrap

                    clip: true
                    focus: true

                    onActiveFocusChanged:{
                        if(activeFocus) selectAll()
                    }

                    background: Rectangle{
                        color: input.activeFocus? Qt.darker(palette.highlight, 1.5): input.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                        border.width: input.hovered || input.active? 1:0
                        border.color: palette.highlight
                        opacity: enabled? 0.7 : 0.3
                    }
                }
            }

            XsLabel {
                Layout.alignment: Qt.AlignRight
                text: "Frame Rate"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.alignment: Qt.AlignLeft
                XsComboBox {
                    id: rateCB
                    Layout.preferredHeight: 24
                    model: helpers.mediaRates()
                    onModelChanged: currentIndex = rateCB.model.indexOf(sessionRate.value)
                }
                XsInfoButton {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    tooltipText: "This sets the frame rate for the timeline."
                }
            }

            /*XsLabel {
                Layout.alignment: Qt.AlignRight
                text: "Re-speed Clips"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.alignment: Qt.AlignLeft
                XsCheckBox {
                    id: re_speed
                }
                XsInfoButton {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    tooltipText: "If this button is checked, media whose natural frame rate doesn't match the timeline rate will be re-sped so that media frames map 1:1 with timeline frames. So media that is 30fps will play at double speed if the timeline is 60fps. Otherwise the frames from the underlying media undergo a video pull-down to match the timeline frame rate."

                }
            }*/

        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Repeater {
                model: popup.choices

                XsSimpleButton {
                    text: popup.choices[index]
                    //width: XsStyleSheet.primaryButtonStdWidth*2
                    Layout.alignment: Qt.AlignRight
                    onClicked: {
                        popup.response(input.text, rateCB.currentText, popup.choices[index])
                        popup.visible = false
                    }
                }
            }
        }
    }
}