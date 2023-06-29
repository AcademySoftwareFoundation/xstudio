// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

XsToolbarItem  {

    id: scrubber
    display_value: parseFloat(value).toFixed(float_display_decimals)
    property var min_: float_scrub_min ? float_scrub_min : 0.0
    property var max_: float_scrub_max ? float_scrub_max : 1.0
    property var float_scrub_step_: float_scrub_step ? float_scrub_step : 0.1
    property var float_scrub_sensitivity_: float_scrub_sensitivity ? float_scrub_sensitivity : 0.1
    fixed_width_font: true
    hovered: mouse_area.containsMouse
    property var saved_value: undefined

    showHighlighted: mouse_area.containsMouse | mouse_area.pressed | (activated != undefined && activated)

    MouseArea {

        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        property int start_drag_mouse_x: 0
        property real start_drag_value: 0

        onPressed: {
            start_drag_mouse_x = mouseX
            start_drag_value = value
            widget_clicked()
        }

        onMouseXChanged: {
            if (pressed) {
                var tmp = Math.round(
                    (start_drag_value + (mouseX-start_drag_mouse_x)*scrubber.float_scrub_sensitivity_)/scrubber.float_scrub_step_
                    )*scrubber.float_scrub_step_;
                tmp = Math.max(Math.min(tmp, max_), min_);
                value = tmp;
            }
        }

        onDoubleClicked: {
            if (default_value != undefined && value !== default_value) {
                scrubber.saved_value = value
                value = default_value
            } else if (scrubber.saved_value) {
                value = scrubber.saved_value
            }
        }

    }

}
