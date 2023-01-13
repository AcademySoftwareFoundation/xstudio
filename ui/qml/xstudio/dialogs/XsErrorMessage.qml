// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialog {
    property alias okay_text: btnOK.text
    property alias text: text_control.text

    property alias textHorizontalAlignment: text_control.horizontalAlignment
    property alias textVerticalAlignment: text_control.verticalAlignment
    property alias textFontSize: text_control.font.pixelSize

    minimumHeight: 300
    minimumWidth: 300

    keepCentered: true
    centerOnOpen: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Flickable {
            id: flickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            // width: parent.width
            height: Math.min(contentHeight, 300)
            width: Math.min(Math.max(contentWidth, 100), 800)
            contentWidth: text_control.implicitWidth
            contentHeight: text_control.implicitHeight

            TextArea.flickable: TextArea {
                id: text_control
                text: "test"
                // wrapMode: Text.WordWrap
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                font.pixelSize: 14
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                readOnly: true
                // textFormat: Text.RichText
                horizontalAlignment : TextEdit.AlignHCenter
                verticalAlignment : TextEdit.AlignVCenter
            }
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
        }

        DialogButtonBox {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 20

            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            RoundButton {
                id: btnOK
                text: qsTr("Close")
                width: 60
                height: 24
                radius: 5
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
                    radius: 5
                    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: btnOK.text
                    color: XsStyle.hoverColor//:XsStyle.mainColor
                    font.family: XsStyle.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: mouseArea
                    hoverEnabled: true
                    anchors.fill: btnOK
                    cursorShape: Qt.PointingHandCursor
                    onClicked: close()
                }
            }
        }
    }
}
