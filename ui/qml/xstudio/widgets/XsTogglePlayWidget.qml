// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {
	text: "Play Forwards"
	icon.source: !playhead ? "qrc:///feather_icons/play.svg" : (playhead.playing ? "qrc:///feather_icons/pause.svg" : "qrc:///feather_icons/play.svg")
    tooltip: !playhead ? "Play forwards." : (playhead.playing ? "Pause playing." : "Play forwards.")	
	onClicked: {

		// when starting playback return to 
		// forwards, normal speed
		if (!playhead.playing) {
			playhead.forward = true
			playhead.velocityMultiplier = 1
		}
		playhead.playing = !playhead.playing

	}
}
