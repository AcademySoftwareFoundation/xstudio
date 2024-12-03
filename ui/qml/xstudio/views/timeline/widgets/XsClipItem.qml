// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

XsGradientRectangle {
	id: control

	property bool isHovered: false
	property bool isEnabled: true
	property bool isLocked: false
	property bool isSelected: false
	property bool isDragging: false

	property bool isMissingMedia: false
	property bool isInvalidRange: false

	property bool showRolling: false
	property bool showDragLeft: false
	property bool showDragRight: false
	property bool showDragLeftLeft: false
	property bool showDragRightRight: false
	property bool showDragMiddle: false

	property int start: 0
	property int duration: 0
	property int availableStart: 0
	property int availableDuration: 1
	property string name
	property int dragValue: 0
	property color primaryColor: defaultClip
	property var realColor: Qt.tint(timelineBackground, helpers.saturate(helpers.alphate(primaryColor, 0.3), 0.3))
    property bool isMoving: false
    property bool isCopying: false
    property color mediaFlagColour: "transparent"

	signal draggingStarted(mode: string)
	signal dragging(mode: string, x: real, y: real)
	signal draggingStopped(mode: string)
	signal doubleTapped(mode: string)
	signal tapped(button: int, x: real, y: real, modifiers: int)

    opacity: isHovered ? 1.0 : isEnabled ? (isLocked ? 0.6 : 1.0) : 0.3

	border.width: 1
    border.color: isHovered ? palette.highlight : (isMissingMedia && isEnabled ? "Red" : (isInvalidRange && isEnabled ? "Orange" : Qt.darker(realColor, 0.8)))

	flatColor: topColor

    topColor: isSelected ? Qt.darker(palette.highlight, 2) : realColor
    bottomColor: isSelected ? Qt.darker(palette.highlight, 2) :  Qt.darker(realColor,1.2)

	XsGradientRectangle {
		anchors.left: parent.left
		anchors.leftMargin: 1
		anchors.topMargin: 1
		anchors.bottomMargin: 1
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		width: 10
		orientation: Gradient.Horizontal
		topColor: mediaFlagColour
		bottomColor: control.bottomColor
		flatTheme: false
		visible: mediaFlagColour != "transparent" && mediaFlagColour != "#00000000"
		opacity: showDragMiddle ? 0.2 : 0.6
	}

	XsElideLabel {
		anchors.fill: parent
		anchors.leftMargin: 11
		anchors.rightMargin: 5
		anchors.bottomMargin: 5
		elide: Qt.ElideMiddle
		text: !isDragging ? name : dragValue > 0 ? "+" + dragValue : dragValue
		z:2
		color: palette.text
	    horizontalAlignment: isDragging ? Text.AlignHCenter : Text.AlignLeft
	    verticalAlignment: isDragging ? Text.AlignVCenter : Text.AlignBottom
		opacity: 0.8
	}

	readonly property int dragWidth: 8

	Rectangle {
		z: 0
		radius: 2
		visible: showDragMiddle
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.topMargin: 2
		anchors.bottomMargin: 2
		anchors.horizontalCenter: parent.horizontalCenter
		width: control.width
		color: "transparent"
		// opacity: hoverMiddleHandler.hovered ? 1.0 : 0.5

        HoverHandler {
        	id: hoverMiddleHandler
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
        	acceptedModifiers: Qt.NoModifier
        	onDoubleTapped: control.doubleTapped("middle")
        	onSingleTapped: {
        		let g = mapToGlobal(0,0)
		        control.tapped(Qt.LeftButton, g.x, g.y, Qt.NoModifier)
        	}
        }

        TapHandler {
        	acceptedModifiers: Qt.ShiftModifier
        	onSingleTapped: {
        		let g = mapToGlobal(0,0)
		        control.tapped(Qt.LeftButton, g.x, g.y, Qt.ShiftModifier)
        	}
        }

        TapHandler {
        	acceptedModifiers: Qt.ControlModifier
        	onSingleTapped: {
        		let g = mapToGlobal(0,0)
		        control.tapped(Qt.LeftButton, g.x, g.y, Qt.ControlModifier)
        	}
        }

        DragHandler {
            cursorShape: Qt.PointingHandCursor
            // yAxis.enabled: false

            dragThreshold: 5
            xAxis.minimum: (control.width/2) - dragWidth
            xAxis.maximum: (control.width/2) + dragWidth

            onTranslationChanged: dragging("middle", translation.x, translation.y)
            onActiveChanged: {
            	if(active) {
            		draggingStarted("middle")
            		parent.anchors.horizontalCenter = undefined
            	} else {
            		draggingStopped("middle")
	            	parent.anchors.horizontalCenter = parent.parent.horizontalCenter
	            }
            }
        }
	}

	XsClipDragLeft {
		id: dragLeftIndicator
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		anchors.leftMargin: 1

		visible: (hoveredLeftHandler.hovered && !isDragging) || leftDragHandler.active
		thickness: 2
		width: 6
		color: leftDragHandler.active ? XsStyleSheet.accentColor : Qt.darker(XsStyleSheet.accentColor,1.2)
		shadowColor: palette.text

	}

	Item {
		visible: showDragLeft
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		anchors.leftMargin : showDragLeftLeft ? dragWidth : 0

		width: dragWidth

        HoverHandler {
        	id: hoveredLeftHandler
            cursorShape: Qt.PointingHandCursor
        }

        // modifies the activeStart / activeDuration frame, bounded by duration>0 and availableStartFrame
        // also bounded by precedding item being a gap.

        DragHandler {
        	id: leftDragHandler
            cursorShape: Qt.PointingHandCursor
            yAxis.enabled: false
            dragThreshold: 1
            onTranslationChanged: dragging("left", translation.x, 0)
            xAxis.minimum: (dragWidth * 2) - 1
            xAxis.maximum: (dragWidth * 2) + 1
            onActiveChanged: {
            	if(active) {
            		draggingStarted("left")
            		parent.anchors.left = undefined
            	} else {
            		draggingStopped("left")
	            	parent.anchors.left = parent.parent.left
	            }
            }
        }
	}

	XsClipDragRight {
		id: dragRightIndicator
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.right: parent.right
		anchors.rightMargin: 1

		visible: (hoveredRightHandler.hovered  && !isDragging)|| rightDragHandler.active
		thickness: 2
		width: 6
		shadowColor: palette.text
		color: rightDragHandler.active ? XsStyleSheet.accentColor : Qt.darker(XsStyleSheet.accentColor,1.2)
	}

	Item {
		visible: showDragRight
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.right: parent.right
		anchors.rightMargin : showDragRightRight ? dragWidth : 0

		width: dragWidth

        HoverHandler {
        	id: hoveredRightHandler
            cursorShape: Qt.PointingHandCursor
        }

        DragHandler {
        	id: rightDragHandler

            cursorShape: Qt.PointingHandCursor
            yAxis.enabled: false
            dragThreshold: 1

            xAxis.minimum: control.width - (dragWidth * 3) - 1
            xAxis.maximum: control.width - (dragWidth * 3) + 1

            onTranslationChanged: dragging("right", translation.x, 0)
            onActiveChanged: {
            	if(active) {
            		draggingStarted("right")
            		parent.anchors.right = undefined
            	} else {
            		draggingStopped("right")
	            	parent.anchors.right = parent.parent.right
	            }
            }
        }
   	}

	XsClipDragBoth {
		id: dragLeftLeftIndicator
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		x: -width/2

		visible: (hoveredDragLeftLeft.hovered  && !isDragging) || leftLeftDragHandler.active
		thickness: 2
		gap: 4
		width: 16
		color: leftLeftDragHandler.active ? XsStyleSheet.accentColor : Qt.darker(XsStyleSheet.accentColor,1.2)
		shadowColor: palette.text
	}

	Item {
		visible: showDragLeftLeft
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		width: dragWidth

        HoverHandler {
        	id: hoveredDragLeftLeft
            cursorShape: Qt.PointingHandCursor
        }

        DragHandler {
        	id: leftLeftDragHandler
            yAxis.enabled: false
            dragThreshold: 1
            xAxis.minimum: - 1
            xAxis.maximum: 1
            onTranslationChanged: dragging("leftleft", translation.x, 0)
            onActiveChanged: {
            	if(active) {
            		draggingStarted("leftleft")
            		parent.anchors.left = undefined
            	} else {
            		draggingStopped("leftleft")
	            	parent.anchors.left = parent.parent.left
	            }
            }
        }
	}

	Item {
		visible: showDragRightRight
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.right: parent.right
		width: dragWidth

        HoverHandler {
        	id: hoveredDragRightRight
            cursorShape: Qt.PointingHandCursor

            onHoveredChanged: {
            	if(!isDragging && !rightRightDragHandler.active) {
	            	if(hovered) {
						dragRightRightIndicator.parent = control.parent.parent.parent.parent
	            	} else {
						dragRightRightIndicator.parent = parent
	            	}
	            }
            }
        }

		XsClipDragBoth {
			id: dragRightRightIndicator
			visible: (hoveredDragRightRight.hovered && !isDragging) || rightRightDragHandler.active
			anchors.top: parent.top
			anchors.bottom: parent.bottom
			x: control.mapToItem(parent, 0, 0).x +  (control.width - width/2)
			thickness: 2
			gap: 4
			width: 16
			color: rightRightDragHandler.active ? XsStyleSheet.accentColor : Qt.darker(XsStyleSheet.accentColor,1.2)
			shadowColor: palette.text
		}

        DragHandler {
        	id: rightRightDragHandler
            cursorShape: Qt.PointingHandCursor
            yAxis.enabled: false
            dragThreshold: 1
            xAxis.minimum: control.width - dragWidth
            xAxis.maximum: control.width
            onTranslationChanged: dragging("rightright", translation.x, 0)
            onActiveChanged: {
            	if(active) {
            		dragRightRightIndicator.parent = control.parent.parent.parent.parent
            		parent.anchors.right = undefined
            		draggingStarted("rightright")
            	} else {
            		draggingStopped("rightright")
            		dragRightRightIndicator.parent = parent
	            	parent.anchors.right = parent.parent.right
	            }
            }
        }
	}

	Rectangle {
		radius: 2
		visible: showRolling

		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: parent.left
		anchors.topMargin: 1
		anchors.bottomMargin: 1
		anchors.leftMargin: (control.width / availableDuration) * (start - availableStart)

		width: (control.width / availableDuration) * duration
		color: palette.highlight
		opacity: rollHandler.hovered ? 1.0 : 0.5

        HoverHandler {
        	id: rollHandler
            cursorShape: Qt.PointingHandCursor
        }
        DragHandler {
            cursorShape: Qt.PointingHandCursor
            yAxis.enabled: false
            dragThreshold: 1
            xAxis.minimum: 0
            xAxis.maximum: control.width - parent.width
            onTranslationChanged: dragging("roll", translation.x * (availableDuration/duration), 0)
            onActiveChanged: {
            	if(active) {
            		draggingStarted("roll")
            		parent.anchors.left = undefined
            	} else {
            		draggingStopped("roll")
	            	parent.anchors.left = parent.parent.left
	            }
            }
        }
	}
}
