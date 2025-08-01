// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Shapes 1.12

Shape {
	id: control
	property color color: "black"
	property real thickness: 1.0

	property bool topBorder: true
	property bool bottomBorder: true
	property bool leftBorder: true
	property bool rightBorder: true

	readonly property real halfThick: thickness / 2


	ShapePath {
		strokeWidth: control.thickness
		strokeColor: topBorder ? control.color : "transparent"
		fillColor: "transparent"

        startX: halfThick
        startY: halfThick

		PathLine {x: control.width - 1 - halfThick ; y: halfThick }
	}

	ShapePath {
		strokeWidth: control.thickness
		strokeColor: bottomBorder ? control.color : "transparent"
		fillColor: "transparent"

        startX: halfThick
        startY: control.height-1 - halfThick

		PathLine {x: control.width-1-halfThick; y: control.height-1-halfThick}
	}

	ShapePath {
		strokeWidth: control.thickness
		strokeColor: leftBorder ? control.color : "transparent"
		fillColor: "transparent"

        startX: halfThick
        startY: halfThick

		PathLine {x: halfThick; y: control.height-1-halfThick}
	}

	ShapePath {
		strokeWidth: control.thickness
		strokeColor: rightBorder ? control.color : "transparent"
		fillColor: "transparent"

        startX: control.width-1-halfThick
        startY: halfThick

		PathLine {x: control.width-1-halfThick; y: control.height-1-halfThick}
	}

}