// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

Item {
	property bool enableBuffering: false
	property var source: null
	property var value: null

	property int delay: 0

	property var updateTimer: Timer {
        interval: delay
        running: false
        repeat: false
        onTriggered: {
            value = source
        }
    }

	onSourceChanged: {
		if(enableBuffering && delay) {
			if(!updateTimer.running)
                updateTimer.start()
		}
		else
			value = source
	}

	Component.onCompleted: {
		value = source
	}
}