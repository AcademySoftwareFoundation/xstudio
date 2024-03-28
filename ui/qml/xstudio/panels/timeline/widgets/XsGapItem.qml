// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.1

Rectangle {
	id: control

	property bool isHovered: false
	property bool isEnabled: true
	property bool isSelected: false
	property int start: 0
	property int parentStart: 0
	property int duration: 0
	property real fps: 24.0
	property string name
    readonly property bool extraDetail: isSelected && height > 60

	color: timelineBackground

	// XsTickWidget {
	// 	anchors.left: parent.left
	// 	anchors.right: parent.right
	// 	anchors.top: parent.top
	// 	height: Math.min(parent.height/5, 20)
	// 	start: control.start
	// 	duration: control.duration
	// 	fps: fps
	// 	endTicks: false
	// }

	XsElideLabel {
		anchors.fill: parent
		anchors.leftMargin: 5
		anchors.rightMargin: 5
	    horizontalAlignment: Text.AlignHCenter
	    verticalAlignment: Text.AlignVCenter
		text: name
		opacity: 0.4
		elide: Qt.ElideMiddle
		font.pixelSize: 14
		clip: true
		visible: isHovered
		z:1
	}
	Label {
		anchors.horizontalCenter: parent.horizontalCenter
		text: duration
		anchors.top: parent.top
		anchors.topMargin: 5
		z:2
		visible: extraDetail
	}
	Label {
		anchors.top: parent.top
		anchors.left: parent.left
		anchors.topMargin: 5
		anchors.leftMargin: 10
		text: start
		visible: extraDetail
		z:2
	}
	Label {
		anchors.verticalCenter: parent.verticalCenter
		anchors.left: parent.left
		anchors.leftMargin: 10
		text: parentStart
		visible: isHovered
		z:2
	}
	Label {
		anchors.verticalCenter: parent.verticalCenter
		anchors.right: parent.right
		anchors.rightMargin: 10
		text: parentStart + duration - 1
		visible: isHovered
		z:2
	}
	Label {
		anchors.top: parent.top
		anchors.topMargin: 5
		anchors.right: parent.right
		anchors.rightMargin: 10
		text: start + duration - 1
		z:2
		visible: extraDetail
	}
}
