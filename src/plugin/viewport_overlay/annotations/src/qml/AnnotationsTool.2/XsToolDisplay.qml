// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0



import xStudio 1.0
import xstudio.qml.models 1.0

GridLayout {

    id: toolProperties
    
    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 6

    Text{
        text: "Display"
        font.pixelSize: fontSize
        font.family: fontFamily
        color: toolInactiveTextColor
        elide: Text.ElideRight
        horizontalAlignment: horizontal ? Text.AlignRight : Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight*4 : -1
        Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
        Layout.fillWidth: horizontal ? false : true
        Layout.fillHeight: horizontal ? true : false
    }

    // Sigh - hooking up the draw mode backen attr to the combo box here
    // is horrible! Need something better than this!
    XsModuleData {
        id: annotations_tool_draw_mode_model
        modelDataName: "annotations_tool_draw_mode"
    }

    XsAttributeValue {
        id: __display_mode
        attributeTitle: "Display Mode"
        model: annotations_tool_draw_mode_model
    }
    property alias display_mode: __display_mode.value

    XsButtonWithImageAndText{ 
        
        id: displayBtn
        iconText: display_mode.substring(display_mode.length-6, display_mode.length)
        Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1
        Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
        Layout.fillWidth: horizontal ? false : true
        Layout.fillHeight: horizontal ? true : false
        iconSrc: display_mode == "Always" ? "qrc:///anno_icons/check_circle.svg" : "qrc:///anno_icons/pause_circle.svg"
        textDiv.visible: true
        textDiv.font.bold: false
        textDiv.font.pixelSize: XsStyleSheet.fontSize
        paddingSpace: 4

        onClicked:{
            if(displayMenu.visible) displayMenu.visible = false
            else{
                displayMenu.x = x
                displayMenu.y = y + height
                displayMenu.visible = true
            }
        }

    }

    XsPopupMenu {
        id: displayMenu
        visible: false
        menu_model_name: "displayMenu" + toolProperties
    }
    XsMenuModelItem {
        text: "Always"
        menuPath: ""
        menuCustomIcon: "qrc:///anno_icons/check_circle.svg"
        menuItemPosition: 1
        menuModelName: displayMenu.menu_model_name
        onActivated: {
            display_mode = "Always"
        }
    }
    XsMenuModelItem {
        text: "When Paused"
        menuPath: ""
        menuCustomIcon: "qrc:///anno_icons/pause_circle.svg"
        menuItemPosition: 2
        menuModelName: displayMenu.menu_model_name
        onActivated: {
            display_mode = "Only When Paused"
        }
    }


}

