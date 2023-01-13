// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    title: qsTr("Repeat Mode")
    hasCheckable: true
    id: repeat_menu

    ListModel {
		id: items
 	    ListElement {
	        text: "Play Once"
	        abbr: "Play Once"
	        value: 0
	        icon: 'qrc:///feather_icons/log-in.svg'
	    }
 	    ListElement {
	        text: "Loop"
	        abbr: "Loop"
	        value: 1
	        icon: 'qrc:///feather_icons/rotate-ccw.svg'
	    }
 	    ListElement {
	        text: "Ping Pong"
	        abbr: "Ping Pong"
	        value: 2
	        icon: 'qrc:///feather_icons/refresh-cw.svg'
	    }
	}

    Instantiator {
        model: items
        delegate: XsMenuItem {
			mytext: abbr
			mycheckable: true
            mychecked: playhead ? value === playhead.loopMode : 0
            onTriggered: {
				if (playhead) {
					playhead.loopMode = value
				}
            }
        }
        onObjectAdded: repeat_menu.insertItem(index, object)
    	onObjectRemoved: repeat_menu.removeItem(object)
    }

}
