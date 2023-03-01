// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

import xstudio.qml.helpers 1.0

//    Layout 3 widgets as follows with animated hide/show
//    and resize slider bars
//
//    |----------------------------------------------|
//    |           |                                  |
//    |           |                                  |
//    |      A    |                                  |
//    |           |                                  |
//    |           |                                  |
//    |           |                C                 |
//    |-----------|                                  |
//    |           |                                  |
//    |           |                                  |
//    |     B     |                                  |
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
    property var child_widget_C
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
        return (widget == child_widget_A || widget == child_widget_B || widget == child_widget_C)
    }

    XsSplitWidget {

        id: main_splitter
        anchors.fill: parent
        widgetA: a_and_b
        widgetB: child_widget_C
        active: the_root.active
        index: 0
        prefs_store_object: the_root.prefs
        is_horizontal: false

        XsSplitWidget {
            active: the_root.active
            id: a_and_b
            widgetA: child_widget_A
            widgetB: child_widget_B
            index: 1
            prefs_store_object: the_root.prefs
            is_horizontal: true

        }

    }

    function toggle_widget_vis(widget) {

        if (widget.visible) hide_widget(widget)
        else unhide_widget(widget)

    }

    function hide_widget(widget) {
        // can't hide if it's the only visible widget
        if (widget == child_widget_A) {
            if (!child_widget_B.visible && !child_widget_C.visible) return
            main_splitter.hide_widget(child_widget_A)
        } else if (widget == child_widget_B) {
            if (!child_widget_A.visible && !child_widget_C.visible) return
            if (child_widget_C.visible) {
                b_and_c.hide_widget(child_widget_B)
            } else {
                main_splitter.hide_widget(b_and_c)
            }
        } else if (widget == child_widget_C) {
            if (!child_widget_A.visible && !child_widget_B.visible) return
            if (child_widget_B.visible) {
                b_and_c.hide_widget(child_widget_C)
            } else {
                main_splitter.hide_widget(b_and_c)
            }
        }
    }

    function unhide_widget(widget) {
        // can't hide if it's the only visible widget
        if (widget == child_widget_A) {
            main_splitter.unhide_widget(child_widget_A)
        } else {
            main_splitter.unhide_widget(b_and_c)
            b_and_c.unhide_widget(widget)
        }
    }

}
