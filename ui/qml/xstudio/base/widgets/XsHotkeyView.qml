// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

Rectangle {

	id: control
    color: "transparent"

	border.color : XsStyle.menuBorderColor
	border.width : 1

    width: 72+128+256+8

    Text {
        id: seq
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 7
        width: 72
        text: sequence
        color: XsStyle.controlColor
        font.family: XsStyle.controlTitleFontFamily
        font.pixelSize: XsStyle.popupControlFontSize
	}

    Rectangle {
        id: divider
        color: XsStyle.menuBorderColor
        anchors.left: seq.right
        y: 4
        height: parent.height-8
        width: 2
    }

    Text {
        id: cmp
        anchors.left: divider.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 7
        width: 128
        text: component
        color: XsStyle.controlColor
        font.family: XsStyle.controlTitleFontFamily
        font.pixelSize: XsStyle.popupControlFontSize
	}

    Rectangle {
        id: divider2
        color: XsStyle.menuBorderColor
        anchors.left: cmp.right
        y: 4
        height: parent.height-8
        width: 2
    }


    Text {
        id: thetext
        anchors.left: divider2.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 7
        width: 256
        text: hotkey_name
        color: XsStyle.controlColor
        font.family: XsStyle.controlTitleFontFamily
        font.pixelSize: XsStyle.popupControlFontSize
	}

}