// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

XsWindow{
    id: popup

    title: ""
    property string body: ""
    property string initialText: ""
    property var choices: []
    width: 400
    height: area ? 400 : 200
    signal response(variant text, variant button_press)
    property var chaser
    property bool area: false

    ColumnLayout {

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 20

        XsText {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.fillHeight: area ? false : true
            text: body
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.weight: Font.Bold
            font.pixelSize: XsStyleSheet.fontSize*1.2
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.fillHeight: area ? true : false
            color: "transparent"
            border.color: "black"

            Keys.onReturnPressed:{
                popup.response(input.text, popup.choices[popup.choices.length-1])
                popup.visible = false
            }
            Keys.onEscapePressed: {
                popup.response(input.text, popup.choices[0])
                popup.visible = false
            }

            XsTextField{ id: input
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

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Repeater {
                model: popup.choices

                XsSimpleButton {
                    text: popup.choices[index]
                    //width: XsStyleSheet.primaryButtonStdWidth*2
                    Layout.alignment: Qt.AlignRight
                    onClicked: {
                        popup.response(input.text, popup.choices[index])
                        popup.visible = false
                    }
                }
            }
        }
    }
}