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

    Text {
        id: seq
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 7
        text: sequence
        color: XsStyle.controlColor
        font.family: XsStyle.controlTitleFontFamily
        font.pixelSize: XsStyle.popupControlFontSize
	}

    Text {
        id: thetext
        anchors.left: seq.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 7
        text: hotkey_name
        color: XsStyle.controlColor
        font.family: XsStyle.controlTitleFontFamily
        font.pixelSize: XsStyle.popupControlFontSize
	}

}