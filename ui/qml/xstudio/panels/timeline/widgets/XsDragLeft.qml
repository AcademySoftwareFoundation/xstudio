// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Shapes 1.12
import xStudio 1.0


Shape {
	id: control
	property real thickness: 2
	property color color: XsStyle.highlightColor

	ShapePath {
		strokeWidth: control.thickness
		strokeColor: control.color
		fillColor: "transparent"

        startX: 0
        startY: 0

		// to bottom right
		PathLine {x: 0; y: control.height}
	}

	ShapePath {
		strokeWidth: control.thickness
		fillColor: control.color
		strokeColor: control.color

        startX: 0
        startY: control.height / 3

		// to bottom right
		PathLine {x: 0; y: (control.height / 3) * 2}
		PathLine {x: control.width; y: control.height / 2}
		PathLine {x: 0; y: control.height / 3}
	}
}
