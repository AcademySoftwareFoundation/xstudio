// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.0

import xStudio 1.0

Rectangle {

    id: control
    property var value_: value ? value : 0
    color: "transparent"

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        width: height*1.66
        color: value

        border.color: ma.containsMouse ? XsStyleSheet.accentColor : XsStyleSheet.baseColor

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                colorDialog.color = value
                colorDialog.open()
            }
        }

    }

    ColorDialog {
        id: colorDialog
        title: "Choose a colour"
        onAccepted: value = colorDialog.color
    }

}
