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
import xstudio.qml.helpers 1.0

Item{
    id: toolSetBg

    property int rowItemCount: 4 //5
    property real itemWidth: (width-framePadding*2)/rowItemCount
    onItemWidthChanged: {
        buttonWidth = itemWidth
    }
    property real itemHeight: XsStyleSheet.primaryButtonStdHeight

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: annotations_tool_types
        modelDataName: "annotations_tool_settings"
    }

    XsAttributeValue {
        id: tool_types_value
        attributeTitle: "Active Tool"
        model: annotations_tool_types
    }
    XsAttributeValue {
        id: tool_types_choices
        role: "combo_box_options"
        attributeTitle: "Active Tool"
        model: annotations_tool_types
    }

    property alias current_tool: tool_types_value.value

    property alias tool_choices: tool_types_choices.value

    property var toolImages: [
        "qrc:///anno_icons/draw_brush.svg",
        "qrc:///anno_icons/draw_laser.svg",
        "qrc:///anno_icons/draw_shape_square.svg",
        "qrc:///anno_icons/draw_shape_circle.svg",
        "qrc:///anno_icons/draw_shape_arrow.svg",
        "qrc:///anno_icons/draw_shape_line.svg",
        "qrc:///anno_icons/draw_text.svg",
        "qrc:///anno_icons/draw_eraser.svg"
    ]

    GridView{

        id: toolSet

        x: framePadding
        y: framePadding

        width: itemWidth*rowItemCount
        height: itemHeight * 2

        cellWidth: itemWidth
        cellHeight: itemHeight

        interactive: false
        flow: GridView.FlowLeftToRight

        // read only convenience binding to backend.
        currentIndex: tool_choices ? tool_choices.indexOf(current_tool) : undefined

        model: tool_choices // this is 'role data' from the backend attr

        delegate: toolSetDelegate

        Component{

            id: toolSetDelegate

            Rectangle{
                width: toolSet.cellWidth
                height: toolSet.cellHeight
                color: "transparent"

                XsPrimaryButton{ id: toolBtn

                    // x: index>=rowItemCount? width/2:0
                    width: parent.width - itemSpacing //index==(rowItemCount-1)? parent.width : parent.width - itemSpacing
                    height: parent.height - itemSpacing

                    clip: true
                    isToolTipEnabled: false
                    isActive: current_tool===text
                    anchors.top: parent.top

                    text: tool_choices[index]
                    imgSrc: toolImages[index]

                    onClicked: {
                        if(isActive)
                        {
                            //Disables tool by setting the 'value' of the 'active tool'
                            // attribute in the plugin backend to 'None'
                            current_tool = "None"
                        }
                        else
                        {
                            current_tool = text
                        }
                    }

                }

            }
        }

    }

}

