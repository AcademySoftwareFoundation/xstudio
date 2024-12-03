import QtQuick 2.12
import QtGraphicalEffects 1.15

Item {
	id: control
	property real thickness: 1
	property color color: palette.highlight
	property color shadowColor:  "#010101"

	Rectangle {
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		x: 0
		width: thickness
		color: control.color
	}

	Rectangle {
		x: thickness
		anchors.top: parent.top
		height: thickness
		width: control.width - thickness
		color: control.color
	}
	Rectangle {
		x: thickness
		anchors.bottom: parent.bottom
		height: thickness
		width: control.width - thickness
		color: control.color
	}

    layer.enabled: true
    layer.effect: DropShadow{
        verticalOffset: 0
        horizontalOffset: 0
        color: control.shadowColor
        radius: 1
        samples: 3
        spread: 1.5
    }

}
