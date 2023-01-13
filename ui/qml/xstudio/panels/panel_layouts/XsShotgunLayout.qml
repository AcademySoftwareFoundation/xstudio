// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

import xstudio.qml.properties 1.0

//    Layout 4 widgets as follows with animated hide/show
//    and resize slider bars
//
//    |----------------------------------------------|
//    |           |                 |                | 
//    |           |                 |                |
//    |      A    |        B        |        C       |
//    |           |                 |                |
//    |           |                 |                |
//    |           |                 |                |
//    |           |----------------------------------|
//    |           |                                  |
//    |           |                                  |
//    |           |                D                 |
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
    property var child_widget_D
    property bool active: false
    visible: active    

    // create a binding to backend prefs to store the divider positions
    XsPreferenceSet {
        id: prefs
        preferencePath: "/ui/qml/" + parent_window_name + "_" + layout_name
    }
    property alias prefs: prefs

    function contains(widget) {
        return (widget == child_widget_A || widget == child_widget_B || widget == child_widget_C)
    }

    XsSplitWidget {

        id: main_splitter
        anchors.fill: parent
        widgetA: child_widget_A
        widgetB: child_widget_BCD
        active: the_root.active
        index: 0
        prefs_store_object: the_root.prefs
        is_horizontal: false

        XsSplitWidget {            
            active: the_root.active
            id: child_widget_BCD
            widgetA: child_widget_BC
            widgetB: child_widget_D
            index: 1
            prefs_store_object: the_root.prefs
            is_horizontal: true

            XsSplitWidget {            
                active: the_root.active
                id: child_widget_BC
                widgetA: child_widget_B
                widgetB: child_widget_C
                index: 2
                prefs_store_object: the_root.prefs
                is_horizontal: false
    
            }

        }
    
    }


}
