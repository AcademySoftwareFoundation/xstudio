// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

import xstudio.qml.module 1.0

XsTrayButton {
	// doesn't work
	anchors.fill: parent
    text: "Shot Browser"
    source: "qrc:///feather_icons/download-cloud.svg"
    tooltip: "Open the Shot Browser Panel."
    buttonPadding: pad
    toggled_on: sessionFunction.object_map["ShotgunRoot"].browser.visible
    onClicked: sessionFunction.object_map["ShotgunRoot"].toggle_browser()
}