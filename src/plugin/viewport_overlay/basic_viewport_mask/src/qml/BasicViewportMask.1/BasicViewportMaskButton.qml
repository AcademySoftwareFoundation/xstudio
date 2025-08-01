// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

XsToolbarItem  {

    id: control
    property var value_: value ? value : ""
    title_: "Mask (M)"
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
        width: combo_group.width
        height: combo_group.height + settings_button.height + 20
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

        ListView {

            id: combo_group

            // N.B the 'combo_box_options' attribute is propagated via the 
            // XsToolBox which instantiates the BasicViewportMaskButton
            model: combo_box_options 
            height: count * XsStyle.menuItemHeight
            width: 100
            y: 5

            delegate: Rectangle {

                id: checkBoxItem
                // Icon (radio or checkbox)
                anchors.left: parent.left
                height: XsStyle.menuItemHeight
                width: popup.width

                property var checked: thetext.text === control.value_
                property var highlighted: mouse_area.containsMouse

                color: highlighted ? XsStyle.highlightColor : "transparent"
                gradient: styleGradient.accent_gradient

                Text {
                    id: thetext
                    anchors.left: iconbox.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 7
                    text: (combo_box_options && index >= 0) ? combo_box_options[index] : ""
                    color: XsStyle.controlColor
                    font.family: XsStyle.controlTitleFontFamily
                    font.pixelSize: XsStyle.popupControlFontSize
                }

                Rectangle {

                    id: iconbox
                    width: iconsize
                    height: iconsize

        //            color: menuItem.checked?XsStyle.highlightColor:"transparent"
                    color: checkBoxItem.checked ? XsStyle.highlightColor : "transparent"
        
                    anchors.left: parent.left
                    anchors.leftMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                                        //            visible: menuItem.checkable
                    border.width: 1 // mycheckableDecorated?1:0
                    border.color: checkBoxItem.checked ? XsStyle.hoverColor : checkBoxItem.highlighted ? XsStyle.hoverColor : XsStyle.mainColor

                    radius: iconsize / 2
                    Image {
                        id: checkIcon
                        visible: checkBoxItem.checked
                        sourceSize.height: XsStyle.menuItemHeight
                        sourceSize.width: XsStyle.menuItemHeight
                        source: "check"
                        width: iconsize // 2
                        height: iconsize // 2
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
                        value = thetext.text
                        popup.visible = false
                    }
    
                }

            }
        
        }

        Rectangle {

            id: divider
            anchors.margins: 5
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: combo_group.bottom
            height: XsStyle.menuBorderWidth
            color: XsStyle.menuBorderColor
        }
        
        Rectangle {

            id: settings_button
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: divider.bottom
            anchors.margins: 5
            height: XsStyle.menuItemHeight
            property var highlighted: mouse_area2.containsMouse
            color: highlighted ? XsStyle.highlightColor : "transparent"
            gradient: styleGradient.accent_gradient

            Text {
                id: thetext
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "Settings ..."
                color: XsStyle.controlColor
                font.family: XsStyle.controlTitleFontFamily
                font.pixelSize: XsStyle.popupControlFontSize
            }

            MouseArea {
                id: mouse_area2
                hoverEnabled: true
                anchors.fill: parent
                onClicked: {
                    launchSettingsDlg()
                    popup.visible = false
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

    function launchSettingsDlg() {
        dynamic_widget = Qt.createQmlObject('import xStudio 1.0; XsModuleAttributesDialog { title: \"Viewport Mask Settings"; attributesGroupNames: "viewport_mask_settings"}', settings_button)
        dynamic_widget.raise()
        dynamic_widget.show()
    }        

}
