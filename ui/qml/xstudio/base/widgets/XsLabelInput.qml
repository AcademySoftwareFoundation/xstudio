// SPDX-License-Identifier: Apache-2.0
// Comprised of 2 pieces:
// 1. HEADER (Rect + Text)
// 2. ListView (driven by Models) that uses Delegates to display a row of buttons

import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import xStudio 1.0

XsTextInput {
	id: control

    leftPadding: 6
    rightPadding: 6
    topPadding: 2
    bottomPadding:2

	Rectangle {
		id: background
		anchors.fill: parent
		color: palette.base
		z: parent.z - 1
		border.width: parent && parent.activeFocus ? 2 : 1
		border.color: parent && parent.activeFocus ? palette.highlight : palette.button
	}
	Connections {
   //      function onActiveFocusChanged() {
			// // background.color = (activeFocus ? XsStyle.highlightColor : palette.base)
			// // control.color = (activeFocus ? "black" : XsStyle.controlTitleColor)
   //  	}
    	function onEditingFinished() {
	    	parent.forceActiveFocus()
    	}
    }
}