// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.module 1.0

XsToolbarItem  {

    id: control
    value_text: value ? "On" : "Off"
    title_: "HUD"
    hovered: mouse_area.containsMouse
    showHighlighted: mouse_area.containsMouse | mouse_area.pressed | (activated != undefined && activated)
    property int iconsize: XsStyle.menuItemHeight *.66

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            toggleShow()
            widget_clicked()
        }
    }

    property alias popup_win: popup

    Rectangle {

        id: popup
        width: column2.width
        height: column1.height + column2.height + 20
        visible: false
        z: 10000

        onVisibleChanged: {
            if (!visible) {
                sessionWidget.deactivateWidgetHider()
            }
        }

        // we have to make this a child of the sessionWidget to allow us to 
        // use a mouseArea that covers the sessionWidget to close this pop-up
        // when the user clicks outside the pop-up.
        parent: sessionWidget

        border {
            color: XsStyle.menuBorderColor
            width: XsStyle.menuBorderWidth
        }
        color: XsStyle.mainBackground
        radius: XsStyle.menuRadius

        ColumnLayout {

            id: column1
            anchors.top: divider.bottom
            anchors.topMargin: 4
            anchors.left: parent.left
            anchors.right: parent.right
            Repeater
            {
                model: ["Enable HUD"] 
                Rectangle {

                    id: checkBoxItem
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    height: XsStyle.menuItemHeight
                    implicitHeight: XsStyle.menuItemHeight
    
                    property var checked: value
                    property var highlighted: mouse_area.containsMouse
    
                    color: highlighted ? XsStyle.highlightColor : "transparent"
                    gradient: styleGradient.accent_gradient
    
                    Text {
                        id: thetext
                        anchors.left: iconbox.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 7
                        text: "HUD Enabled"
                        color: XsStyle.controlColor
                        font.family: XsStyle.controlTitleFontFamily
                        font.pixelSize: XsStyle.popupControlFontSize
                    }
    
                    Rectangle {
    
                        id: iconbox
                        width: iconsize
                        height: iconsize
    
                        color: checkBoxItem.checked ? XsStyle.highlightColor : "transparent"
            
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        border.width: 1
                        border.color: checkBoxItem.checked ? XsStyle.hoverColor : checkBoxItem.highlighted ? XsStyle.hoverColor : XsStyle.mainColor
    
                        radius: iconsize / 2
                        Image {
                            id: checkIcon
                            visible: checkBoxItem.checked
                            sourceSize.height: XsStyle.menuItemHeight
                            sourceSize.width: XsStyle.menuItemHeight
                            source: "qrc:///feather_icons/check.svg"
                            width: iconsize
                            height: iconsize
                            anchors.centerIn: parent
                        }
                        ColorOverlay{
                            id: colorolay
                            anchors.fill: checkIcon
                            source:checkIcon
                            visible: checkBoxItem.checked
                            color: checkBoxItem.highlighted?XsStyle.hoverColor:XsStyle.hoverColor
                            antialiasing: true
                        }
                    }
    
                    MouseArea {
                        id: mouse_area
                        hoverEnabled: true
                        anchors.fill: parent
                        onClicked: {
                            value = !value
                            popup.visible = false
                        }
                    }
                } 
            }
            
        }

        Rectangle {

            id: divider
            anchors.margins: 5
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: column2.bottom
            height: XsStyle.menuBorderWidth
            color: XsStyle.menuBorderColor
        }
        
        // note this model only has one item, which is the 'scribble mode' attribute
        // in the backend.
        XsOrderedModuleAttributesModel {
            id: hud_element_toggles
            attributesGroupNames: "hud_element_toggles"
        }

        ColumnLayout {

            id: column2
            anchors.top: parent.top
            anchors.topMargin: 4
            Repeater {

                model: hud_element_toggles
        
                Rectangle {

                    id: checkBoxItem
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    implicitWidth: thetext.implicitWidth + iconsize + 50 + 4
                    implicitHeight: XsStyle.menuItemHeight - 2

                    property var checked: value
                    property var highlighted: mouse_area.containsMouse

                    color: highlighted ? XsStyle.highlightColor : "transparent"
                    gradient: styleGradient.accent_gradient
                    property var hud_plugin_name: title

                    Text {
                        id: thetext
                        anchors.left: iconbox.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 7
                        text: title
                        color: XsStyle.controlColor
                        font.family: XsStyle.controlTitleFontFamily
                        font.pixelSize: XsStyle.popupControlFontSize
                    }

                    Rectangle {

                        id: iconbox
                        width: iconsize
                        height: iconsize

                        color: checkBoxItem.checked ? XsStyle.highlightColor : "transparent"
            
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        border.width: 1
                        border.color: checkBoxItem.checked ? XsStyle.hoverColor : checkBoxItem.highlighted ? XsStyle.hoverColor : XsStyle.mainColor

                        radius: iconsize / 2
                        Image {
                            id: checkIcon
                            visible: checkBoxItem.checked
                            sourceSize.height: XsStyle.menuItemHeight
                            sourceSize.width: XsStyle.menuItemHeight
                            source: "qrc:///feather_icons/check.svg"
                            width: iconsize
                            height: iconsize
                            anchors.centerIn: parent
                        }
                        ColorOverlay{
                            id: colorolay
                            anchors.fill: checkIcon
                            source:checkIcon
                            visible: checkBoxItem.checked
                            color: checkBoxItem.highlighted?XsStyle.hoverColor:XsStyle.hoverColor
                            antialiasing: true
                        }
                    }

                    Rectangle {

                        id: iconbox2
                        width: iconsize
                        height: iconsize
                        color: "transparent"
                        property var highlighted2: mouse_area2.containsMouse
                        visible: disabled_value == false
            
                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            id: settingsIcon
                            anchors.fill: parent
                            sourceSize.height: XsStyle.menuItemHeight
                            sourceSize.width: XsStyle.menuItemHeight
                            source: "qrc:///feather_icons/settings.svg"
                            layer {
                                enabled: true
                                effect: ColorOverlay {
                                    color: iconbox2.highlighted2 ? "white" : XsStyle.controlTitleColor
                                }
                            }
                        }
                        MouseArea {
                            id: mouse_area2
                            hoverEnabled: true
                            anchors.fill: parent

                            XsModuleAttributesDialog {
                                id: hud_item_settings_dialog
                                title: hud_plugin_name + " HUD Settings"
                                attributesGroupNames: hud_plugin_name + " Settings"
                            }
            
                            onClicked: {
                                hud_item_settings_dialog.raise()
                                hud_item_settings_dialog.show()
                                hud_item_settings_dialog.raise()
                                popup.visible = false                                            
                            }
                        }
                    }

                    MouseArea {
                        id: mouse_area
                        hoverEnabled: true
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: iconbox2.left
                        onClicked: {
                            value = !value
                        }
                    }
                } 
            }
        }
    }


    function toggleShow() {
        if(popup.visible) {
            popup.visible = false
        }
        else{
            var popup_coords = mapToGlobal(0,0)
            popup_coords = sessionWidget.mapFromGlobal(popup_coords.x,popup_coords.y)
            popup.y = popup_coords.y-popup.height
            popup.x = popup_coords.x
            popup.visible = true
            sessionWidget.activateWidgetHider(hideMe)
        }
    }

    function hideMe() {
        popup.visible = false
    }
}
