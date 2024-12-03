import QtQuick 2.12
import xStudio 1.0

Item {
	id: control
	property real thickness: 2
	property real gap: 4
	property color color: palette.highlight
	property color shadowColor:  "#010101"

	XsClipDragLeft {
		anchors.left: parent.left
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		thickness: control.thickness
		color: control.color
		shadowColor: control.shadowColor
		width: (control.width - control.gap)/2
	}

	XsClipDragRight {
		anchors.right: parent.right
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		thickness: control.thickness
		color: control.color
		shadowColor: control.shadowColor
		width: (control.width - control.gap)/2
	}
}

