// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {
    
	text: "Snapshot"
	icon.source: "qrc:///feather_icons/camera.svg"
    tooltip: "Capture and save the viewer image to file."
    onClicked: toggleSnapshotDialog()

}