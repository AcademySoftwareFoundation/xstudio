// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

Rectangle {
	id: control
    property alias source: image.source
    property string toolTip: ""
    property string toolTipTitle: ""

	signal clicked(variant mouse)
	signal pressed(variant mouse)
	signal released(variant mouse)

    gradient: mouse_area.containsMouse ? styleGradient.accent_gradient : null
    color: mouse_area.containsMouse ? XsStyle.highlightColor : XsStyle.mainBackground

	border.color : XsStyle.menuBorderColor
	border.width : 1
    Image {
    	id: image
        anchors.margins: 2
        anchors.fill: parent
		source: "qrc:///feather_icons/more-horizontal.svg"
        sourceSize.height: height
        sourceSize.width:  width
        smooth: true
	    layer {
	        enabled: true
	        effect: ColorOverlay {
	            color: XsStyle.controlColor
	        }
	    }
	}
    MouseArea {
    	id: mouse_area
        cursorShape: Qt.PointingHandCursor
       	hoverEnabled: true
        anchors.fill: parent

    	onClicked: control.clicked(mouse)
    	onReleased: control.released(mouse)
		onPressed: control.pressed(mouse)

        onHoveredChanged: {
            if(toolTip) {
                if (containsMouse) {
                    status_bar.normalMessage(toolTip, toolTipTitle)
                } else {
                    status_bar.clearMessage()
                }
            }
        }
    }
}