// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Shapes 1.12
import xStudio 1.0

Item {
    id: control

    property alias thickness: line.width
    property color color: playheadActive ? palette.highlight : Qt.darker(palette.base, 1.8)

    property int position: start
    property int start: 0
    property int duration: 0
    property int secondOffset: 0
    property real fractionOffset: 0
    property real fps: 24
    property real tickWidth: (control.width / duration)

    readonly property real cursorX: playheadActive ? liveCursorX || 0: stoppedCursorX || 0
    readonly property real liveCursorX: ((position-start) * tickWidth) - fractionOffset
    property bool playheadActive: timelinePlayhead.pinnedSourceMode ? currentPlayhead.uuid == timelinePlayhead.uuid : false
    property var stoppedCursorX
    onPlayheadActiveChanged: {
        if (!playheadActive) stoppedCursorX = liveCursorX
    }

    property bool showBobble: true

    // layer.enabled: true
    // layer.samples: 4

    Rectangle {
        id: line
        width: 1
        color: control.color

        x: cursorX
        height: parent.height
    }

    Rectangle {
        width: 7.0
        color: control.color
        visible: showBobble
        x: cursorX-3
        height: 4
    }

    // Note: the qml Shape stuff REALLY slows down drawing of the interface.
    // Avoid!

    // ShapePath {
    //     strokeWidth: control.thickness
    //     fillColor: control.color
    //     strokeColor: control.color

    //     startX: cursorX-(cursorSize/2)
    //     startY: 0

    //     // to bottom right
    //     PathLine {x: cursorX+(cursorSize/2); y: 0}
    //     PathLine {x: cursorX; y: cursorSize}
    //     // PathLine {x: cursorX-(cursorSize/2); y: 0}
    // }
}