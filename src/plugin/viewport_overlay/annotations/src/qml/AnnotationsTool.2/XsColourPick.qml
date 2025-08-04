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
    
    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 1

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: colour_pick_prefs
        modelDataName: "annotations_colour_picker_prefs"
    }

    /* Here we locate particular nodes in the annotations_model_data giving
    convenient access to backend data. Seems crazy but this is the QML way! */
    XsAttributeValue {
        id: __average_mode
        attributeTitle: "Colour Pick Average"
        model: colour_pick_prefs
    }
    property alias average_mode: __average_mode.value

    XsAttributeValue {
        id: __show_magnifier
        attributeTitle: "Colour Pick Show Magnifier"
        model: colour_pick_prefs
    }
    property alias show_magnifier: __show_magnifier.value

    XsAttributeValue {
        id: __hide_drawings
        attributeTitle: "Colour Pick Hide Drawings"
        model: colour_pick_prefs
    }
    property alias hide_drawings: __hide_drawings.value

    Item{ 

        id: colorProp

        Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight*4 : -1
        Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
        Layout.fillWidth: horizontal ? false : true
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

    RowLayout {

        Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1
        Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
        Layout.fillWidth: horizontal ? false : true
        Layout.fillHeight: horizontal ? true : false
        spacing: itemSpacing

        XsPrimaryButton{ 

            Layout.fillWidth: true
            Layout.fillHeight: true
                
            imgSrc: "qrc:///icons/search.svg"
            isActive: show_magnifier
            onClicked: show_magnifier = !show_magnifier
            toolTip: "Enable/disable the pixel magnifier helper in the xSTUDIO viewport."

        }

        XsPrimaryButton{ 
                    
            Layout.fillWidth: true
            Layout.fillHeight: true
    
            imgSrc: "qrc:///icons/visibility.svg"
            isActive: !hide_drawings
            onClicked: hide_drawings = !hide_drawings
            toolTip: "Drawings visibility during pixel-picking: when the pixel picker is active, if you want annotations to be hidden DESELECT this option."

        }

        XsPrimaryButton{ 

            Layout.fillWidth: true
            Layout.fillHeight: true
                
            imgSrc: "qrc:///icons/functions.svg"
            isActive: average_mode
            onClicked: average_mode = !average_mode
            toolTip: "When this is active, the picked colour is the cumulative average of all pixels that the mouse pointer has scrubbed over. Otherwise the instantaneous pixel value under the pointer is the picked colour."

        }

    }

}

