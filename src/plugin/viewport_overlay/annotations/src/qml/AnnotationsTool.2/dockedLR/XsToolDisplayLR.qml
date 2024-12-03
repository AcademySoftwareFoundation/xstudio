// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3
import QtGraphicalEffects 1.15

import xStudio 1.0
import xstudio.qml.models 1.0

Item{ id: toolActionsDiv

    property alias displayBtn: displayBtn

    Rectangle{ id: dispText
        x: framePadding
        width: parent.width - x*2
        height:  XsStyleSheet.primaryButtonStdHeight/1.5
        color: "transparent"

        Text{
            text: "Display"
            font.pixelSize: fontSize
            font.family: fontFamily
            color: toolInactiveTextColor
            width: parent.width
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: framePadding
        }
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

    XsButtonWithImageAndText{ id: displayBtn
        iconText: "Always"
        x: framePadding
        width: parent.width- x*2
        height: XsStyleSheet.primaryButtonStdHeight
        anchors.top: dispText.bottom
        iconSrc: "qrc:///anno_icons/check_circle.svg"
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

        Component.onCompleted: {
            display_mode = "Always"
        }
    }

    XsPopupMenu {
        id: displayMenu
        visible: false
        menu_model_name: "displayMenu" + toolActionsDiv
    }
    XsMenuModelItem {
        text: "Always"
        menuPath: ""
        menuCustomIcon: "qrc:///anno_icons/check_circle.svg"
        menuItemPosition: 1
        menuModelName: displayMenu.menu_model_name
        onActivated: {
            display_mode = text
            displayBtn.iconText = text
            displayBtn.iconSrc = menuCustomIcon
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
            displayBtn.iconText = "Paused"
            displayBtn.iconSrc = menuCustomIcon
        }
    }


}

