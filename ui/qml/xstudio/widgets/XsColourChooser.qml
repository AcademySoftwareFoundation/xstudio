// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

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
                colourDialog.currentColour = value
                colourDialog.show()
            }
        }

    }

    XsColourDialog {
        id: colourDialog
        title: "Choose a colour"
        onAccepted: value = colourDialog.currentColour
    }

}
