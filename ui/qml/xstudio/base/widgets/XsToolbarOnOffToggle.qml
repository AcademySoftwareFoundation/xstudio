// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

XsToolbarItem  {

    display_value: value === true ? "On" : value === false ? "Off" : value
    hovered: mouse_area.containsMouse

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (!inactive) value = !value
            widget_clicked()
        }
    }


}