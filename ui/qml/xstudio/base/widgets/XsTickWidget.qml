// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.0

import xStudio 1.1

Rectangle {
	id: control
	property int start: 0
    property int duration: 0
    property int secondOffset: 0
    property real fractionOffset: 0
    property real fps: 24
    property real tickWidth: (control.width / duration)
	property color tickColor: "black"
	property bool renderFrames: duration > 2 && tickWidth > 5
	property bool renderSeconds: duration > fps && tickWidth * fps > 5
    property bool endTicks: true

	color: "transparent"

    signal frameClicked(int frame)
    signal framePressed(int frame)
    signal frameDragging(int frame)

    MouseArea{
        id: mArea
        anchors.fill: parent
        hoverEnabled: true
        property bool dragging: false
        onClicked: {
            if (mouse.button == Qt.LeftButton) {
                frameClicked(start + ((mouse.x + fractionOffset)/ tickWidth))
            }
        }
        onReleased: {
            dragging = false
        }
        onPressed: {
            if (mouse.button == Qt.LeftButton) {
                framePressed(start + ((mouse.x + fractionOffset)/ tickWidth))
                dragging = true
            }
        }

        onPositionChanged: {
            if (dragging) {
                frameDragging(start + ((mouse.x + fractionOffset)/ tickWidth))
            }
        }
    }


	// frame repeater
    Repeater {
        model: control.height > 8 && renderFrames ? duration-(endTicks ? 0 : 1) : null
        Rectangle {
            height: control.height / 2
            color: tickColor

            x: ((index+(endTicks ? 0 : 1)) * tickWidth) - fractionOffset
            visible: x >=0
            width: 1
        }
    }

    Repeater {
        model: control.height > 4 && renderSeconds ? Math.ceil(duration / fps) - (endTicks ? 0 : 1) : null
        Rectangle {
            height: control.height
            color: tickColor

            x: (((index + (endTicks ? 0 : 1)) * (tickWidth * fps)) - (secondOffset * tickWidth)) - fractionOffset
            visible: x >=0
            width: 1
        }
    }
}