// SPDX-License-Identifier: Apache-2.0

import QtQuick

MouseArea {
    anchors.fill: parent
    z: 100
	cursorShape: undefined

	property var target: hotkey_area != undefined  ? hotkey_area : parent

    onPressed: (event) => {
        event.accepted = false
        target.forceActiveFocus()
    }
}