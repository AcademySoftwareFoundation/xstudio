// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

XsBufferedProperty {

	// Note: this helper is intended to prevent QML graph evaluation/redraw
	// happening out-of-sync with the screen refresh when using remote
	// desktop technology like PCOIP that limits refresh to 30Hz.
	//
	// QML can execute UI redraws at much faster rate than 60Hz. It either 
	// redraws when the xSTUDIO viewport tells it to, or when the QML graph
	// needs to be re-evaluated due to some graphical property changing.
	//
	// We found that Qt will order a redraw due to the playhead UI element moving
	// during playback and this will happen out-of-sync with the viewport
	// refresh timing. As such, when media is playing at 24fps you can get 
	// dropped frames because the UI is re-drawn due to the playhead item changing
	// but the on-screen media frame has not changed (yet) fro 'N', say. 
	// PCOIP grabs the frame buffer and we have lost the chance to update the 
	// on-screen framefor another 33ms.... by which time if we are not lucky we 
	// actually need to show media frame N+2.

	property bool playing: false
	delay: updateDelay.value
	updateTimer: appWindow.bufferedUITimer

	enableBuffering: playing && enableDelay.value

	Connections{
		target: updateTimer
		function onTriggered() {
			value = source
		}
	}

    XsPreference {
        id: updateDelay
        path: "/core/playhead/remote_desktop_optimisation_delay"
    }
    XsPreference {
        id: enableDelay
        path: "/core/playhead/remote_desktop_optimisation"
    }
}
