import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle {

    id: container
    anchors.fill: parent
    color: "transparent"

    property var panels_layout_model
    property var panels_layout_model_index
    property var panels_layout_model_row

    property var panel_name: panel_source_qml ? panel_source_qml: undefined
    property var previous_panel_name
    property var the_panel

    onPanel_nameChanged: {
        if (panel_name) loadPanel()
        else the_panel = undefined
    }

    // we use the id of this item to create a unique string which identifies
    // this instance of the XsViewContainer. This is used as the name for the
    // model data that constructs the menu for this particular XsViewContainer
    property string panel_id: "" + container

    XsPanelsMenuButton {
        panel_id: container.panel_id
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        z: 1000
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: the_panel === undefined
        Text {
            anchors.centerIn: parent
            text: "Empty"
            color: "white"
        }
    }

    function loadPanel() {

        if (panel_name && panel_name == previous_panel_name) return;

        let component = Qt.createComponent(panel_name)
        previous_panel_name = panel_name
        if (component.status == Component.Ready) {

            if (the_panel != undefined) the_panel.destroy()
            the_panel = component.createObject(
                container,
                {
                })

        } else {
            console.log("Error loading panel:", component, component.errorString())
        }
    }

    XsMenuModelItem {
        text: "Split Panel Horizontally"
        menuPath: ""
        menuModelName: panel_id
        onActivated: {
            split_panel(true)
        }
    }

    XsMenuModelItem {
        text: "Split Panel Vertically"
        menuPath: ""
        menuModelName: panel_id
        onActivated: {
            split_panel(false)
        }
    }


    XsMenuModelItem {
        text: "Undock Panel"
        menuPath: ""
        menuModelName: panel_id
        onActivated: {
            undock_panel()
        }
    }

    XsMenuModelItem {
        text: "Close Panel"
        menuPath: ""
        menuModelName: panel_id
        onActivated: {
            panels_layout_model.removeRows(panels_layout_model_row, 1, panels_layout_model_index.parent)
        }
    }

    XsViewsModel {
        id: views_model
    }

    Repeater {
        model: views_model
        Item {
            XsMenuModelItem {
                text: view_name
                menuPath: "Set Panel To|"
                menuModelName: panel_id
                onActivated: {
                    switch_panel_to(view_qml_path)
                }
            }
        }    
    }


    function undock_panel() {

    }

    function split_panel(horizontal) {

        var split_direction = horizontal ? "widthwise" : "heightwise"

        container.panel_name = "XsSplitPanel"
        var parent_split_type = panels_layout_model.get(panels_layout_model.index(0, 0, panels_layout_model_index.parent), "split")
        if (parent_split_type == "tbd") {
            panels_layout_model.set(panels_layout_model.index(0, 0, panels_layout_model_index.parent), split_direction, "split")
        }
        if (parent_split_type == split_direction) {

            panels_layout_model.insertRows(panels_layout_model_row, 1, panels_layout_model_index.parent)
            var new_row_count = panels_layout_model.rowCount(panels_layout_model_index.parent)+1
            for(let i=0; i<new_row_count; i++) {
                panels_layout_model.set(panels_layout_model.index(i, 0, panels_layout_model_index.parent), i/new_row_count, "fractional_position")
                panels_layout_model.set(panels_layout_model.index(i, 0, panels_layout_model_index.parent), 1.0/new_row_count, "fractional_size")
            }

        } else {

            panels_layout_model.set(panels_layout_model_index, split_direction, "split")
            panels_layout_model.insertRows(0, 2, panels_layout_model_index)
            panels_layout_model.set(panels_layout_model.index(0, 0, panels_layout_model_index), 0.0, "fractional_position")
            panels_layout_model.set(panels_layout_model.index(0, 0, panels_layout_model_index), 0.5, "fractional_size")
            panels_layout_model.set(panels_layout_model.index(1, 0, panels_layout_model_index), 0.5, "fractional_position")
            panels_layout_model.set(panels_layout_model.index(1, 0, panels_layout_model_index), 0.5, "fractional_size")

        }
        container.parent.resizeWidgets()
    }

    function switch_panel_to(qml_file_path) {

        panel_source_qml = qml_file_path

    }

}