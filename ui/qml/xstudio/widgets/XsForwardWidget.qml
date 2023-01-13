// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {
	text: "Forward"
	icon.source: "qrc:///feather_icons/fast-forward.svg"
    tooltip: "Fast Forwards.  Press again for 4x, 8x, 16x speed."
	
	onClicked: {

		if (!playhead.playing) {
			playhead.velocityMultiplier = 1
			playhead.playing = true
			playhead.forward = true
		} else if (!playhead.forward) {
			playhead.forward = true
			playhead.velocityMultiplier = 1;
		} else if (playhead.velocityMultiplier < 16) {
			playhead.velocityMultiplier = playhead.velocityMultiplier*2;
		}
		
	}
}