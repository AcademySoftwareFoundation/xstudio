// SPDX-License-Identifier: Apache-2.0
import QtQml.Models 2.12
import QtQuick 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsTrayButton {

	id: control
	text: "Loop Mode"
	icon.source: menu_items.get(currentIndex)["icon"]
    tooltip: "Select playback loop mode - play through and stop, loop or ping-pong (bounce) loop."
	
	property var playheadLoopMode : playhead ? playhead.loopMode : 1
	property int currentIndex: 0

	onPlayheadLoopModeChanged : {
		currentIndex = playheadLoopMode
	}

    onCurrentIndexChanged : {
		if (playhead) {
			playhead.loopMode = currentIndex
		}
	}
	
	onClicked: {
		collapsePopup.toggleShow()
	}

	XsMenu {
        // x: control.x
        y: control.y - height
	    ActionGroup {id: myActionGroup}
	    hasCheckable: true

        id:collapsePopup
        title: parent.text
        Instantiator {
            model: menu_items
            XsMenuItem {
                mytext: model.text
                myicon: typeof(model.icon) === "undefined" ? "" : model.icon
                mycheckable: true
            	actiongroup: myActionGroup
            	mychecked: index === control.currentIndex
                onTriggered: { control.currentIndex = index }
            }
            onObjectAdded: {
                collapsePopup.insertItem(index, object)
            }
            onObjectRemoved: collapsePopup.removeItem(object)
        }
        function toggleShow() {
        	if(visible) {
        		close()
        	}
        	else{
        		open()
        	}
        }
    }

    ListModel {
		id: menu_items
 	    ListElement {
	        text: "Play Once"
	        abbr: "Play Once"
	        value: 0
	        icon: '../../icons/repeat_mode_play_once.png'
	    }
 	    ListElement {
	        text: "Loop"
	        abbr: "Loop"
	        value: 1
	        icon: '../../icons/repeat_mode_loop.png'
	    }
 	    ListElement {
	        text: "Ping Pong"
	        abbr: "Ping Pong"
	        value: 2
	        icon: '../../icons/repeat_mode_ping_pong.png'
	    }
	}
}


