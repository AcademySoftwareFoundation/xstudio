// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {
	text: "Next Frame"
	icon.source: "qrc:///feather_icons/skip-forward.svg"
    tooltip: "Next frame."	
	onClicked: {
		playhead.step(1)
	}
}
