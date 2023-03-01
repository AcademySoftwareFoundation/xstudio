// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

import xstudio.qml.helpers 1.0

//    Layout 4 widgets as follows with animated hide/show
//    and resize slider bars
//
//    |----------------------------------------------|
//    |           |                |                 |
//    |           |                |                 |
//    |     A     |        B       |       C         |
//    |           |                |                 |
//    |           |                |                 |
//    |           |                |                 |
//    |----------------------------------------------|
//    |                                              |
//    |                                              |
//    |                     D                        |
//    |                                              |
//    |                                              |
//    |----------------------------------------------|
//
//


Rectangle {

    color: "#00000000"
    id: the_root
    property var layout_name: "browse_layout"
    property var parent_window_name: "main_window"
    property var child_widget_A
    property var child_widget_B
    property var child_widget_C
    property var child_widget_D
    property bool active: false
    visible: active

    // create a binding to backend prefs to store the divider positions

    XsModelNestedPropertyMap {
        id: prefs
        index: app_window.globalStoreModel.search_recursive("/ui/qml/" + parent_window_name + "_" + layout_name, "pathRole")
        property alias properties: prefs.values
    }

    property alias prefs: prefs

    function contains(widget) {
        return (widget == child_widget_A || widget == child_widget_B || widget == child_widget_C || widget == child_widget_D)
    }

    XsSplitWidget {

        id: main_splitter
        anchors.fill: parent
        widgetA: a_and_b_and_c
        widgetB: child_widget_D
        active: the_root.active
        is_horizontal: true
        index: 0
        prefs_store_object: the_root.prefs

        XsSplitWidget {

            id: a_and_b_and_c
            widgetA: child_widget_A
            widgetB: b_and_c
            active: the_root.active
            is_horizontal: false
            index: 1
            prefs_store_object: the_root.prefs

            XsSplitWidget {
                active: the_root.active
                id: b_and_c
                widgetA: child_widget_B
                widgetB: child_widget_C
                is_horizontal: false
                index: 2
                prefs_store_object: the_root.prefs
            }
        }

    }

    function toggle_widget_vis(widget) {

        if (widget.visible) hide_widget(widget)
        else unhide_widget(widget)

    }

    function hide_widget(widget) {
        // can't hide if it's the only visible widget
        main_splitter.hide_widget(widget)
    }

    function unhide_widget(widget) {
        // can't hide if it's the only visible widget
        main_splitter.unhide_widget(widget)
    }

}
