// SPDX-License-Identifier: Apache-2.0
import QtQuick

Item {

	property bool enableBuffering: false
	property var source: null
	property var value: null

	property int delay: 0

	property var updateTimer

    onEnableBufferingChanged: {
    	if(!delay && !enableBuffering)
    		value = source
    } 

	onSourceChanged: {
		if(enableBuffering) {
			if(delay && !updateTimer.running) {
				console.log(delay, updateTimer.running)
                updateTimer.start()
			}
		}
		else
			value = source
	}

	Component.onCompleted: { 
		value = source
	}
}