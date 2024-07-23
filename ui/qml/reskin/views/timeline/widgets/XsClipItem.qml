// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

Rectangle {
	id: control

	// clip:true
	property bool isHovered: false
	property bool isEnabled: true
	property bool isFocused: false
	property bool isSelected: false
	property int parentStart: 0
	property int start: 0
	property int duration: 0
	property int availableStart: 0
	property int availableDuration: 1
	property real fps: 24.0
	property string name
	property color primaryColor: defaultClip
    property bool isMoving: false
    property bool isCopying: false
    property color mediaFlagColour: "transparent"

    readonly property bool extraDetail: isHovered && height > 60

    property color mainColor: Qt.lighter( primaryColor, isSelected ? 1.4 : 1.0)

	color: Qt.tint(timelineBackground, helpers.saturate(helpers.alphate(mainColor, 0.3), 0.3))

	opacity: isEnabled ? 1.0 : 0.2

	// XsTickWidget {
	// 	anchors.left: parent.left
	// 	anchors.right: parent.right
	// 	anchors.top: parent.top
	// 	height: Math.min(parent.height/5, 20)
	// 	start: control.start
	// 	duration: control.duration
	// 	fps: control.fps
	// 	endTicks: false
	// }

	Rectangle {
		color: "transparent"
		z:5
		anchors.fill: parent
		border.width: isHovered ? 3 : 2
		border.color: isMoving || isCopying || isFocused ? "red" : isHovered ? palette.highlight : Qt.lighter(
			Qt.tint(timelineBackground, helpers.saturate(helpers.alphate(mainColor, 0.4), 0.4)),
			1.2)
	}

	Rectangle {
		anchors.left: parent.left
		anchors.leftMargin: 2
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		width: 2
		color: mediaFlagColour
		// z: 6
	}

	XsElideLabel {
		anchors.fill: parent
		anchors.leftMargin: 5
		anchors.rightMargin: 5
		elide: Qt.ElideMiddle
		text: name
		opacity: 0.8
		font.pixelSize: 14
		z:1
		clip: true
	    horizontalAlignment: Text.AlignHCenter
	    verticalAlignment: Text.AlignVCenter
	}

	Label {
		anchors.verticalCenter: parent.verticalCenter
		text: parentStart
		anchors.left: parent.left
		anchors.leftMargin: 10
		visible: isHovered
		z:2
	}

	Label {
		anchors.verticalCenter: parent.verticalCenter
		text: parentStart + duration -1
		anchors.right: parent.right
		anchors.rightMargin: 10
		visible: isHovered
		z:2
	}

	Label {
		text: duration
		anchors.top: parent.top
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.topMargin: 5
		visible: extraDetail
		z:2
	}
	Label {
		anchors.left: parent.left
		anchors.leftMargin: 10
		anchors.topMargin: 5
		text: start
		visible: extraDetail
		z:2
	}
	Label {
		anchors.top: parent.top
		anchors.right: parent.right
		anchors.rightMargin: 10
		anchors.topMargin: 5
		text: start + duration - 1
		visible: extraDetail
		z:2
	}

	Label {
		text: availableDuration
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 5
		visible: extraDetail
		opacity: 0.5
		z:2
	}
	Label {
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		anchors.leftMargin: 10
		anchors.bottomMargin: 5
		text: availableStart
		visible: extraDetail
		opacity: 0.5
		z:2
	}
	Label {
		anchors.bottom: parent.bottom
		anchors.right: parent.right
		anchors.rightMargin: 10
		anchors.bottomMargin: 5
		opacity: 0.5
		text: availableStart + availableDuration - 1
		visible: extraDetail
		z:2
	}


	// position of clip in media
	Rectangle {

		visible: isHovered

		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: parent.left

		color: Qt.darker( control.color, 1.2)

		width: (parent.width / availableDuration) * (start - availableStart)
	}

	Rectangle {
		visible: isHovered

		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.right: parent.right

		color: Qt.darker( control.color, 1.2)

		width: parent.width - ((parent.width / availableDuration) * duration) - ((parent.width / availableDuration) * (start - availableStart))
	}
}
