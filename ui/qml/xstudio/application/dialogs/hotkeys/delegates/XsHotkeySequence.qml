// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xStudio 1.0

RowLayout {

    property var regularKeys: []
    property var modifiers: []
    property real fontSize: XsStyleSheet.fontSize*1.5
    property var borderSize: 4
    property var unbound: JSON.stringify(regularKeys) === JSON.stringify([""])
    
    Rectangle {
        Layout.preferredWidth: textField.width + borderSize*2
        Layout.preferredHeight: textField.height + borderSize*2
        color: unbound ? "transparent" : XsStyleSheet.panelTitleBarColor
        border.color: XsStyleSheet.menuBorderColor
        border.width: unbound ? 0 : 1
        radius: 4
        XsText {
            id: textField
            anchors.centerIn: parent
            text: unbound ? "UNBOUND" : regularKeys.length ? regularKeys[0] : ""
            font.family: XsStyleSheet.fixedWidthFontFamily
            font.bold: true
            font.italic: unbound
            font.pixelSize: fontSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }                
    }

    XsText {
        text: modifiers.length ? "+" : ""
        font.family: XsStyleSheet.fixedWidthFontFamily
        font.bold: true
        font.pixelSize: fontSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Repeater {
        model: modifiers
        RowLayout {
            Rectangle {
                Layout.preferredWidth: textField.width + borderSize*2
                Layout.preferredHeight: textField.height + borderSize*2
                color: XsStyleSheet.panelTitleBarColor
                border.color: XsStyleSheet.menuBorderColor
                border.width: 1
                radius: 4
                XsText {
                    id: textField
                    anchors.centerIn: parent
                    text: modelData
                    font.family: XsStyleSheet.fixedWidthFontFamily
                    font.bold: true
                    font.pixelSize: fontSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            XsText {
                text: index < modifiers.length - 1 ? "+" : ""
                font.family: XsStyleSheet.fixedWidthFontFamily
                font.bold: true
                font.pixelSize: fontSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

    }
}