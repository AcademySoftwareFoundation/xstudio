// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {
	text: "Rewind"
	icon.source: "qrc:///feather_icons/rewind.svg"
    //icon.color: XsStyle.controlTitleColor

	// checked: mediaDialog.visible
    tooltip: "Rewind.  Press again for 4x, 8x, 16x speed."
	
	onClicked: {

		if (!playhead.playing) {
			playhead.velocityMultiplier = 1
			playhead.playing = true
			playhead.forward = false
		} else if (playhead.forward) {
			playhead.forward = false
			playhead.velocityMultiplier = 1;
		} else if (playhead.velocityMultiplier < 16) {
			playhead.velocityMultiplier = playhead.velocityMultiplier*2;
		}
	}
}
