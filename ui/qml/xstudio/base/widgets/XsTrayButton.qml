// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

import QtQml 2.14
import QtQuick 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12

Rectangle {

	id: the_button
	color: "transparent"
	property bool mouseHovered: mouseArea.containsMouse
	property var text
	property string tooltip
	property string source
    property bool prototype: false
    property bool toggled_on
    property alias icon: icon
    property int buttonPadding: 0
    width: XsStyle.transportHeight
    signal clicked

	Rectangle {
        anchors.fill: parent
        visible: mouseHovered
        gradient: prototype ? null : styleGradient.accent_gradient
        color: prototype ? XsStyle.indevColor : "white"
	}

	Image {

        id: icon
		anchors.centerIn: parent
		source: the_button.source
		smooth: true
        transformOrigin: Item.Center
        sourceSize.height: XsStyle.toolsButtonIconSize
        sourceSize.width: XsStyle.toolsButtonIconSize
		width: XsStyle.toolsButtonIconSize
		height: XsStyle.toolsButtonIconSize
        property var color: prototype ? XsStyle.indevColor : "white"

		layer {
			enabled: true
			effect: ColorOverlay {
				color: mouseHovered ? "white" : prototype ? XsStyle.indevColor : XsStyle.controlTitleColor
			}
		}
	}

	Rectangle {
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		anchors.right: parent.right
		height: 3
		radius: 2
		color: XsStyle.highlightColor
		gradient: styleGradient.accent_gradient
		visible: toggled_on
	}

	onMouseHoveredChanged: {
        if (mouseHovered) {
            status_bar.normalMessage(tooltip, text, prototype)
        } else {
            status_bar.clearMessage()
        }
    }

	MouseArea {
		id: mouseArea
		anchors.fill: parent
		hoverEnabled: true
		acceptedButtons: Qt.LeftButton | Qt.RightButton
		onClicked: {
			the_button.clicked()
		}
	}
}
