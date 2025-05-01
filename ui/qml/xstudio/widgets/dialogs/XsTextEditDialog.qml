// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xStudio 1.0

XsWindow {
    id: popup

    width: 400
    height: 400
    property alias text: flick.edittext

    signal accepted()

    ColumnLayout {

        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 20

        Flickable {
            id: flick
       
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: edit.paintedWidth
            contentHeight: edit.paintedHeight
            clip: true
       
            function ensureVisible(r)
            {
                if (contentX >= r.x)
                    contentX = r.x;
                else if (contentX+width <= r.x+r.width)
                    contentX = r.x+r.width-width;
                if (contentY >= r.y)
                    contentY = r.y;
                else if (contentY+height <= r.y+r.height)
                    contentY = r.y+r.height-height;
            }

            property alias edittext: edit.text
       
            TextEdit {
                id: edit
                width: flick.width
                focus: true
                wrapMode: TextEdit.Wrap
                onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)
                font.bold: true
                font.family: XsStyleSheet.fixedWidthFontFamily
                color: palette.text
                onVisibleChanged: {
                    if (visible) {
                        text = popup.text
                    }
                }
            
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            XsSimpleButton {
                text: "Cancel"
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    popup.visible = false
                }
            }
            XsSimpleButton {
                text: "Ok"
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    accepted()
                    popup.visible = false
                }
            }
        }
    }
}