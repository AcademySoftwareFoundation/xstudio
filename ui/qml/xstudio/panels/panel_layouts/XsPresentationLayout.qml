// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0


// Layout a single widget! Almost pointless except
// we can follow the pattern for other workspacce
// layouts as they are used by XsStudioWidget

Rectangle {

    color: "#00000000"
    property var layout_name: "presentation_layout"
    property var parent_window_name: "main_window"
    property var child_widget_A
    property bool active: false
    visible: active

    function contains(widget) {
        return (widget == child_widget_A)
    }

    Component.onCompleted: {
        arrange()
    }

    onActiveChanged: {
        if (active) {
            arrange()
        }
    }

    onWidthChanged: arrange()
    onHeightChanged: arrange()

    function arrange() {

        if (!active) return

        if (child_widget_A) {
            child_widget_A.x = x
            child_widget_A.y = y
            child_widget_A.height = height
            child_widget_A.width = width
        }
    }
}
