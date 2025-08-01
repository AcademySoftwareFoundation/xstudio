// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsToolbarItem  {

    id: control
    property var value_: value ? value : ""
    hovered: mouse_area.containsMouse
    showHighlighted: mouse_area.containsMouse | mouse_area.pressed | (activated != undefined && activated)

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (!inactive) popup.toggleShow()
            widget_clicked()
        }
    }

    XsMenu {
        // x: control.x
        y: control.y - height
	    ActionGroup {id: myActionGroup}
        hasCheckable: true
        
        id: popup
        title: ""

        Instantiator {
            model: combo_box_options
            XsMenuItem {
                mytext: (combo_box_options && index >= 0) ? combo_box_options[index] : ""
                mycheckable: true
            	actiongroup: myActionGroup
            	mychecked: mytext === control.value_
	            enabled: combo_box_options_enabled ? combo_box_options_enabled.length == combo_box_options.length ? combo_box_options_enabled[index] : true : true
                onTriggered: { value = mytext }
            }
            onObjectAdded: {
                popup.insertItem(index, object)
            }
            onObjectRemoved: popup.removeItem(object)
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
}
