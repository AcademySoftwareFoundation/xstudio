// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

GridLayout {

    id: toolSetBg

    columns: horizontal ? -1 : 2
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 1

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: annotations_tool_types
        modelDataName: "annotations_tool_settings"
    }

    property var toolImages: [
        "qrc:///anno_icons/draw_brush.svg",
        "qrc:///anno_icons/draw_laser.svg",
        "qrc:///anno_icons/draw_shape_square.svg",
        "qrc:///anno_icons/draw_shape_circle.svg",
        "qrc:///anno_icons/draw_shape_arrow.svg",
        "qrc:///anno_icons/draw_shape_line.svg",
        "qrc:///anno_icons/draw_text.svg",
        "qrc:///anno_icons/draw_eraser.svg",
        "qrc:///anno_icons/draw_color_pick.svg"
    ]

    property var tootTips: [
        "Free paint strokes",
        "Lazer mode - unrecorded strokes",
        "Square - click & drag to draw rectangles",
        "Circle - click & drag to draw circles",
        "Arrow - click & drag to draw arrows",
        "Line - click & drag to draw lines",
        "Text - click to create editable text caption boxes",
        "Eraser - eraser brush strokes",
        "Colour picker - click and/or drag over image to pick pixel colour for the pen (Hotkey: " + colour_pick_hk.sequence + ")"
    ]

    XsHotkeyReference {
        id: colour_pick_hk
        hotkeyName: "Activate colour picker"
    }

    Repeater {

        model: toolChoices // this is 'role data' from the backend attr

        XsPrimaryButton{ 
            
            id: toolBtn

            Layout.fillHeight: horizontal ? true : false
            Layout.fillWidth: horizontal ? false : true
            Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
            Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight : -1

            clip: true

            isActive: currentTool===modelData

            toolTip: tootTips[index]
            imgSrc: toolImages[index]

            onClicked: {
                if(isActive)
                {
                    //Disables tool by setting the 'value' of the 'active tool'
                    // attribute in the plugin backend to 'None'
                    currentTool = "None"
                }
                else
                {
                    currentTool = modelData
                }
            }

        }

    }

}

