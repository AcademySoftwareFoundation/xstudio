// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

import xstudio.qml.properties 1.0

//    Layout 2 widgets as follows with animated hide/show
//    and resize slider bars
//
//    |----------------------------------------------|
//    |           |                                  |
//    |           |                                  |
//    |           |                                  |
//    |           |                B                 |
//    |           |                                  |
//    |     A     |                                  |
//    |           |                                  |
//    |           |                                  |
//    |           |                                  |
//    |           |                                  |
//    |           |                                  |
//    |----------------------------------------------|
//
// 


Rectangle {

    id: the_root    
    color: "#00000000"
    property var layout_name: ""
    property var parent_window_name: "main_window"
    property var child_widget_A
    property var child_widget_B
    property bool active: false
    visible: active

    // create a binding to backend prefs to store the divider positions
    XsPreferenceSet {
        id: prefs
        preferencePath: "/ui/qml/" + parent_window_name + "_" + layout_name
    }
    property alias prefs: prefs

    function contains(widget) {
        return (widget == child_widget_A || widget == child_widget_B)
    }

    XsSplitWidget {

        id: main_splitter
        anchors.fill: parent
        widgetA: child_widget_A
        widgetB: child_widget_B
        active: the_root.active
        index: 0
        prefs_store_object: the_root.prefs
        is_horizontal: false
    
    }

    function toggle_widget_vis(widget) {

        if (widget.visible) hide_widget(widget)
        else unhide_widget(widget)

    }

    function hide_widget(widget) {
        // can't hide if it's the only visible widget
        if (widget == child_widget_A) {
            if (!child_widget_B.visible) return
            main_splitter.hide_widget(child_widget_A)
        } else if (widget == child_widget_B) {
            if (!child_widget_A.visible) return
            main_splitter.hide_widget(child_widget_B)
        }
    }

    function unhide_widget(widget) {
        // can't hide if it's the only visible widget
        main_splitter.unhide_widget(widget)
    }

}
