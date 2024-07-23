// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.module 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsToolbarItem  {

    id: sourceToolbar
    title_: "Source"
    title_abbr: "Src"
    hovered: mouse_area.containsMouse
    showHighlighted: mouse_area.containsMouse | mouse_area.pressed | (activated != undefined && activated)
    property int iconsize: XsStyle.menuItemHeight *.66
    property string toolbar_name

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            toggleShow()
            widget_clicked()
        }
    }

    XsModuleData {
        id: image_source
        modelDataName: toolbar_name + "_image_source"
        onJsonChanged: {
            curr_image_source_property.index = search_recursive("Source", "title")
        }
    }

    XsModuleData {
        id: audio_source
        modelDataName: toolbar_name + "_audio_source"
        onJsonChanged: {
            curr_audio_source_property.index = search_recursive("Source", "title")
        }
    }


    XsModelProperty {
        id: curr_image_source_property
        role: "value"
        index: image_source.search_recursive("Source", "title")
    }

    XsModelProperty {
        id: curr_audio_source_property
        role: "value"
        index: audio_source.search_recursive("Source", "title")
    }

    property var curr_image_source: curr_image_source_property.value
    property var curr_audio_source: curr_audio_source_property.value

    value_text: curr_image_source != "" ? curr_image_source : curr_audio_source != "" ? curr_audio_source : "None"

    property alias popup_win: popup

    Rectangle {

        id: popup
        implicitWidth: 120
        width: Math.max(audioColumn.width, imageColumn.width)
        height: imageColumn.height + audioColumn.height + 20
        visible: false

        // this is needed to ensure we are above the 'override_mouse_area'
        // in the session widget which captures a click *outside* this popup
        // causing it to hide
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

            id: imageColumn
            anchors.top: parent.top
            anchors.topMargin: 4
            anchors.left: parent.left

            Rectangle {
                id: imageSourceItem
                height: XsStyle.menuItemHeight
                implicitHeight: XsStyle.menuItemHeight
                color: "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Image Source"
                    color: XsStyle.controlTitleColor
                    font.family: XsStyle.controlTitleFontFamily
                    font.pixelSize: XsStyle.popupControlFontSize
                }
            }

            Repeater
            {
                model: image_source

                Repeater
                {
                    model: combo_box_options
                   
                    Rectangle {
                        id: checkBoxItem
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        height: XsStyle.menuItemHeight
                        implicitHeight: XsStyle.menuItemHeight
                        implicitWidth: imageText.implicitWidth + iconsize + 50 + 4
                        property var option_text: combo_box_options[index]
        
                        property var checked: value === option_text
                        property var highlighted: mouse_area.containsMouse
        
                        color: highlighted ? XsStyle.highlightColor : "transparent"
                        gradient: styleGradient.accent_gradient
        
                        Text {
                            id: imageText
                            anchors.left: iconbox.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 7
                            text: option_text
                            color: XsStyle.controlColor
                            font.family: XsStyle.controlTitleFontFamily
                            font.pixelSize: XsStyle.popupControlFontSize
                        }
        
                        Rectangle {
        
                            id: iconbox
                            width: iconsize
                            height: iconsize
                            visible: option_text != "No Video"
        
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
                                source: checkIcon
                                visible: checkBoxItem.checked
                                color: checkBoxItem.highlighted?XsStyle.hoverColor:XsStyle.hoverColor
                                antialiasing: true
                            }
                        }
        
                        MouseArea {
                            id: mouse_area
                            hoverEnabled: option_text != "No Video"
                            anchors.fill: parent
                            onClicked: {
                                if (option_text != "No Video") {
                                    value = option_text
                                    hideMe() 
                                }
                            }
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
            anchors.top: imageColumn.bottom
            height: XsStyle.menuBorderWidth
            color: XsStyle.menuBorderColor
        }
        
        ColumnLayout {

            id: audioColumn
            anchors.top: divider.bottom
            anchors.topMargin: 4
            anchors.left: parent.left

            Rectangle {
                id: audioSourceItem
                width: 60
                height: XsStyle.menuItemHeight
                implicitHeight: XsStyle.menuItemHeight
                color: "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 8
                    text: "Audio Source"
                    color: XsStyle.controlTitleColor
                    font.family: XsStyle.controlTitleFontFamily
                    font.pixelSize: XsStyle.popupControlFontSize
                }
            }

            Repeater {
                model: audio_source

                Repeater {
                    model: combo_box_options

                    Rectangle {
                        id: checkBoxItem
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        implicitWidth: audioText.implicitWidth + iconsize + 50 + 4
                        implicitHeight: XsStyle.menuItemHeight - 2
                        property var option_text: combo_box_options[index]

                        property var checked: value === option_text
                        property var highlighted: mouse_area.containsMouse

                        color: highlighted ? XsStyle.highlightColor : "transparent"
                        gradient: styleGradient.accent_gradient

                        Text {
                            id: audioText
                            anchors.left: iconbox.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 7
                            text: option_text
                            color: XsStyle.controlColor
                            font.family: XsStyle.controlTitleFontFamily
                            font.pixelSize: XsStyle.popupControlFontSize
                        }

                        Rectangle {

                            id: iconbox
                            width: iconsize
                            height: iconsize
                            visible: option_text != "No Audio"

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
                                source: checkIcon
                                visible: checkBoxItem.checked
                                color: checkBoxItem.highlighted?XsStyle.hoverColor:XsStyle.hoverColor
                                antialiasing: true
                            }
                        }

                        MouseArea {
                            id: mouse_area
                            hoverEnabled: option_text != "No Audio"
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            onClicked: {
                                if (option_text != "No Audio") {
                                    value = option_text
                                    hideMe() 
                                }
                            }
                        }
                    }
                } 
            }
        }
    }

    property var sw: sessionWidget.width
    property var sh: sessionWidget.height
    onSwChanged: {
        reposition_popup()
    }

    onShChanged: {
        reposition_popup()
    }

    function reposition_popup() {
        var popup_coords = mapToGlobal(0,0)
        popup_coords = sessionWidget.mapFromGlobal(popup_coords.x,popup_coords.y)
        popup.y = popup_coords.y-popup.height
        popup.x = popup_coords.x+Math.min(0, sourceToolbar.width-popup.width)
    }

    function toggleShow() {
        if(popup.visible) {
            popup.visible = false
        }
        else{
            reposition_popup()
            popup.visible = true
            sessionWidget.activateWidgetHider(hideMe)
        }
    }

    function hideMe() {
        popup.visible = false
    }
}