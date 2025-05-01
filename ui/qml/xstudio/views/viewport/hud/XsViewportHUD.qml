import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "."

Item {

    id: hud
    anchors.fill: parent
    clip: true

    // here we access attribute data that declares a QML item for drawing
    // overlays into the viewport. Attributes that are added to the
    // 'hud_elements_fullscreen' attribute group and that have 'qml_code'
    // role data are instatiated here

    XsModuleData {
        id: fullscreen_overlays
        modelDataName: "hud_elements_fullscreen"
    }

    Repeater {

        anchors.fill: parent
        model: fullscreen_overlays
        delegate: XsHudItem {
            width: parent.width
            height: parent.height
        }
    }

    // connect to the viewport toolbar data
    XsModuleData {
        id: toolbar_data
        modelDataName: view.name + "_toolbar"
    }

    XsAttributeValue {
        id: hud_enabled_prop
        attributeTitle: "HUD"
        model: toolbar_data
    }

    property var hud_visible: hud_enabled_prop.value != undefined ? hud_enabled_prop.value : false
    visible: hud_visible

    property var hud_margin: 10

    // Bottom Left Items

    XsModuleData {
        id: hud_elements_bottom_left
        modelDataName: "HUD Bottom Left"
    }

    Column {

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_bottom_left
            XsHudItem {}

        }
    }


    // Bottom Centre Items

    XsModuleData {
        id: hud_elements_bottom_center
        modelDataName: "HUD Bottom Centre"
    }

    Column {

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_bottom_center
            XsHudItem {}

        }
    }


    // Bottom Rigth Items

    XsModuleData {
        id: hud_elements_bottom_right
        modelDataName: "HUD Bottom Right"
    }

    Column {

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_bottom_right
            XsHudItem {}

        }
    }


    // Top Left Items

    XsModuleData {
        id: hud_elements_top_left
        modelDataName: "HUD Top Left"
    }

    Column {

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_top_left
            XsHudItem {}
        }
    }


    // Top Centre Items

    XsModuleData {
        id: hud_elements_top_center
        modelDataName: "HUD Top Centre"
    }

    Column {

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_top_center
            XsHudItem {}

        }
    }


    // Top Right Items

    XsModuleData {
        id: hud_elements_top_right
        modelDataName: "HUD Top Right"
    }

    Column {

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: hud.hud_margin
        Repeater {

            model: hud_elements_top_right
            XsHudItem {}
        }
    }
}
