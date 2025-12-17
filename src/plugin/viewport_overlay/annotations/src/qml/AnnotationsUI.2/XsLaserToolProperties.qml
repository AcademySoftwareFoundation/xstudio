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

    property ListModel currentColorPresetModel: drawColourPresetsModel

    XsModuleData {
        id: annotations_model_data
        modelDataName: "annotations_tool_settings"
    }

    XsIntegerAttrControl {
        id: sizeProp

        attr_group_model: annotations_model_data
        attr_title: "Pen Size"

        text: "Size"

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
    }

    XsIntegerAttrControl{
        id: opacityProp
        
        attr_group_model: annotations_model_data
        attr_title: "Pen Opacity"

        text: "Opacity"

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
    }

    XsAttributeValue {
        id: pen_colour
        attributeTitle: "Pen Colour"
        model: annotations_model_data
    }
    property alias tool_colour_value: pen_colour.value

    XsColourDialog {
        id: colourDialog
        title: "Please pick a colour"
        property var lastColour
        
        linkColour: tool_colour_value

        onCurrentColourChanged: {
            tool_colour_value = currentColour
        }
        onAccepted: {
            close()
        }
        onRejected: {
            tool_colour_value = lastColour
            close()
        }
        onVisibleChanged: {
            if (visible) {
                currentColour = tool_colour_value
                lastColour = tool_colour_value
            }
        }
    }

    property alias colourDialog: colourDialog

    Item{ 

        id: colourProp

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ?  horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight *3: -1 : 0
        Layout.fillHeight: horizontal ? true : false

        property bool isPressed: false
        property bool isMouseHovered: colorMArea.containsMouse

        XsGradientRectangle {
            anchors.fill: parent
            border.color: colourProp.isMouseHovered ? palette.highlight: "transparent"
            border.width: 1
    
            flatColor: topColor
            topColor: colourProp.isPressed ? palette.highlight : XsStyleSheet.controlColour
            bottomColor: colourProp.isPressed ? palette.highlight : "#1AFFFFFF"
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
                color: tool_colour_value ? tool_colour_value : "grey"
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
                    colourDialog.show()
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
    }
    
}

