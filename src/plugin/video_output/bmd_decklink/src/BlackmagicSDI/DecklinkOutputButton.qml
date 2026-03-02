// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0

import "."

Item {

    id: startStopButton

    Rectangle {
        height: parent.height
        width: parent.width/2
        color: "#900"
        radius: button_radius
        XsText {
            text: "Off"
            anchors.centerIn: parent
            font.weight: Font.Bold
            font.pixelSize: 10
        }
    }

    Rectangle {
        height: parent.height
        width: 10
        color: is_running ? "#090" : "#900"
        x: parent.width/2-button_radius
    }

    Rectangle {
        height: parent.height
        width: parent.width/2
        x: width
        color: "#090"
        radius: button_radius
        XsText {
            text: "On"
            anchors.centerIn: parent
            font.weight: Font.Bold
            font.pixelSize: 10
        }
    }

    Rectangle {
        height: parent.height
        width: parent.width/2
        x: is_running ? 0 : width
        color: "#333"
        radius: button_radius
        border.color: "#888"
        border.width: 2
        Behavior on x {NumberAnimation {duration: 50}}
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: button_radius
        border.color: mouseArea2.containsMouse ? palette.highlight : "transparent"
        border.width: 1
    }

    MouseArea {
        id: mouseArea2
        hoverEnabled: true
        anchors.fill: startStopButton
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            // toggling the start stop value to get a callback on the C++
            // side to toggle SDI output. That actual value of startStop
            // is not important!
            startStop = !startStop
        }
    }
}
