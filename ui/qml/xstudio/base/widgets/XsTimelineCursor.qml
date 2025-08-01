// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Shapes 1.12
import xStudio 1.1

Shape {
    id: control

    property real thickness: 2
    property color color: XsStyle.highlightColor

    property int position: start
    property int start: 0
    property int duration: 0
    property int secondOffset: 0
    property real fractionOffset: 0
    property real fps: 24
    property real tickWidth: (control.width / duration)

    readonly property real cursorX: ((position-start) * tickWidth) - fractionOffset
    property int cursorSize: 20

    visible: position >= start

    ShapePath {
        id: line
        strokeWidth: control.thickness
        strokeColor: control.color
        fillColor: "transparent"

        startX: cursorX
        startY: 0

        // to bottom right
        PathLine {x: cursorX; y: control.height}
    }

    ShapePath {
        strokeWidth: control.thickness
        fillColor: control.color
        strokeColor: control.color

        startX: cursorX-(cursorSize/2)
        startY: 0

        // to bottom right
        PathLine {x: cursorX+(cursorSize/2); y: 0}
        PathLine {x: cursorX; y: cursorSize}
        // PathLine {x: cursorX-(cursorSize/2); y: 0}
    }
}

	// // frame repeater
 //    Rectangle {
 //        anchors.top: parent.top
 //        height: control.height
 //        color: cursorColor
 //        visible: position >= start
 //        x: ((position-start) * tickWidth) - fractionOffset
 //        width: 2
 //    }
