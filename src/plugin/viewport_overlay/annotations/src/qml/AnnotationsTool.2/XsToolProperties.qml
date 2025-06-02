// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import QtQml 2.15

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.bookmarks 1.0

GridLayout {

    id: toolProperties
    property var root

    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 1

    XsModuleData {
        id: annotations_model_data
        modelDataName: "annotations_tool_settings"
    }
    Connections {
        target: annotations_model_data // this bubbles up from XsSessionWindow
        function onJsonChanged() {
            setPropertyIndeces()
        }
    }

    function setPropertyIndeces() {
        draw_pen_size.index = annotations_model_data.searchRecursive("Draw Pen Size", "title")
    }

    /* Here we locate particular nodes in the annotations_model_data giving
    convenient access to backend data. Seems crazy but this is the QML way! */
    XsModelProperty {
        id: draw_pen_size
        role: "value"
    }
 
    XsTextCategories {

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false

        id: textCategories
        visible: (currentTool === "Text")
    }

    XsIntegerAttrControl {
        id: sizeProp
        visible: enabled
        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
        text: (currentTool=="Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")? "Width" : "Size"
        enabled: isAnyToolSelected
        attr_group_model: annotations_model_data
        attr_title: toolSizeAttrName
    }

    XsIntegerAttrControl{
        id: opacityProp
        visible: isAnyToolSelected
        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
        text: "Opacity"
        attr_group_model: annotations_model_data
        attr_title: "Pen Opacity"
        enabled: isAnyToolSelected && currentTool != "Erase"
    }

    XsIntegerAttrControl {
        id: bgOpacityProp
        visible: currentTool === "Text"
        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
            text: "BG Opa."
        attr_group_model: annotations_model_data
        attr_title: "Text Background Opacity"
    }

    Item{ 

        id: colorProp
        visible: (isAnyToolSelected && currentTool !== "Erase") || currentTool === "Colour Picker"
        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight *3: -1 : 0
        Layout.fillHeight: horizontal ? true : false

        property bool isPressed: false
        property bool isMouseHovered: colorMArea.containsMouse

        XsGradientRectangle {

            anchors.fill: parent
            border.color: colorProp.isMouseHovered ? palette.highlight: "transparent"
            border.width: 1
    
            flatColor: topColor
            topColor: colorProp.isPressed ? palette.highlight : XsStyleSheet.controlColour
            bottomColor: colorProp.isPressed ? palette.highlight : "#1AFFFFFF"
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 2
            Layout.margins: 2
            XsLabel {
                text: "Colour"
                color: XsStyleSheet.secondaryTextColor
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: currentToolColour ? currentToolColour : "grey"
                border.width: 1
                border.color: "black"                       
            }
        }

        MouseArea{
            id: colorMArea
            propagateComposedEvents: true
            hoverEnabled: true
            anchors.fill: parent
            onClicked: {
                    parent.isPressed = false
                    colorDialog.show()
            }
            onPressed: {
                    parent.isPressed = true
            }
            onReleased: {
                    parent.isPressed = false
            }
        }
    }

    XsColourPresets{ 
        
        id: row3_colourpresets

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : buttonHeight*2 : 0
        Layout.preferredWidth: visible ? horizontal ? buttonHeight*4 : -1 : 0
        Layout.fillHeight: horizontal ? true : false

        visible: (isAnyToolSelected && currentTool !== "Erase")

    }

    Component.onCompleted: {
        setPropertyIndeces()
    }

}

