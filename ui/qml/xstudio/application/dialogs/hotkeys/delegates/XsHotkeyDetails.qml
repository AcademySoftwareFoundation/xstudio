// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQml.Models
import Qt.labs.qmlmodels
import QtQuick.Controls

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

Rectangle {

    id: root
    //Layout.preferredWidth: widthHint
    height: 26
    color: "transparent"
    border.color: ma2.pressed ? XsStyleSheet.accentColor : "transparent"
    border.width: 1

    /*property var maxWidth: sequence.width + label.width + 20
    onMaxWidthChanged: {
        hintWidth(maxWidth)
    }*/

    MouseArea {
        id: ma2
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            showHotkeySetter(hotkeyUuid)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "white"
        opacity: 0.1
        visible: ma2.containsMouse
    }

    XsLabel {

        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: "" + hotkeyName
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter    

        XsToolTip {
            id: tooltip
            text: hotkeyDescription
            visible: ma2.containsMouse
            maxWidth: root.width
            onVisibleChanged: {
                if (visible) {
                    tooltip.x = ma2.mouseX
                    tooltip.y = ma2.mouseY
                }
            }
        }

    }

    XsHotkeySequence {

        id: sequence
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        regularKeys: keyboardKey
        modifiers: keyModifiers
        fontSize: XsStyleSheet.fontSize
        borderSize: 2
    }

}