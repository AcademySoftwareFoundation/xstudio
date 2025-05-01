// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

XsBufferedProperty {
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
