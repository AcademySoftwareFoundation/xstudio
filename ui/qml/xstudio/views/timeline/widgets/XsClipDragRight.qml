import QtQuick
import QtQuick.Effects

Item {
	id: control
	property real thickness: 1
	property color color: palette.highlight
	property color shadowColor:  "#010101"

	Rectangle {
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		x: control.width - thickness
		width: thickness
		color: control.color
	}

	Rectangle {
		anchors.top: parent.top
		height: thickness
		width: control.width - thickness
		color: control.color
	}
	Rectangle {
		anchors.bottom: parent.bottom
		height: thickness
		width: control.width - thickness
		color: control.color
	}

    layer.enabled: true
	layer.effect: MultiEffect {
		shadowEnabled: true
		shadowColor: control.shadowColor
		shadowVerticalOffset: 0
		shadowHorizontalOffset: 0
		shadowScale: 1.5
		shadowBlur: 0.2
	}
}
