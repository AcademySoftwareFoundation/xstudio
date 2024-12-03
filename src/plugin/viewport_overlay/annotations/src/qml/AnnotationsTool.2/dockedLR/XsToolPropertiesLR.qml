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
    id: toolProperties
    property var root

    Rectangle{ id: row1_categories
        width: toolProperties.width - framePadding*2 
        height: visible? buttonHeight + itemSpacing*2 : 0

        color: "transparent";
        visible: (currentTool == "Text")

        XsTextCategoriesLR {
            id: textCategories
            anchors.fill: parent
            visible: (currentTool === "Text")
        }
    }

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
 
    property alias drawPenSize: draw_pen_size.value

    Grid{id: row2_controls
        x: framePadding
        y: row1_categories.visible? row1_categories.y + row1_categories.height : row1_categories.y
        z: 1

        columns: isDividedToCols? 2 : 1
        spacing: itemSpacing
        flow: Grid.TopToBottom
        width: (toolProperties.width - x*2) 

        property bool isDividedToCols: width>toolPropertiesWidthThreshold
        property real gridItemWidth: isDividedToCols? width/2 : width

        XsIntegerAttrControl {
            id: sizeProp
            visible: enabled
            width: row2_controls.gridItemWidth
            height: visible? buttonHeight : 0
            text: (currentTool=="Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")? "Width" : "Size"
            enabled: isAnyToolSelected
            attr_group_model: annotations_model_data
            attr_title: toolSizeAttrName
        }

        XsIntegerAttrControl{
            id: opacityProp
            visible: isAnyToolSelected
            width: row2_controls.gridItemWidth
            height: visible? buttonHeight : 0
            text: "Opacity"
            attr_group_model: annotations_model_data
            attr_title: "Pen Opacity"
            enabled: isAnyToolSelected && currentTool != "Erase"
        }

        XsIntegerAttrControl {
            id: bgOpacityProp
            visible: currentTool === "Text"
            width: row2_controls.gridItemWidth
            height: visible? buttonHeight : 0
            text: "BG Opa."
            attr_group_model: annotations_model_data
            attr_title: "Text Background Opacity"
        }

        XsViewerMenuButton{ id: colorProp
            visible: enabled
            width: row2_controls.gridItemWidth
            height: visible? buttonHeight : 0
            text: "Colour    "
            shortText: "Col    "
            enabled: (isAnyToolSelected && currentTool !== "Erase")
            showBorder: isMouseHovered
            isActive: isPressed

            property bool isPressed: false
            property bool isMouseHovered: colorMArea.containsMouse

            MouseArea{
                id: colorMArea
                propagateComposedEvents: true
                hoverEnabled: true
                anchors.fill: parent
                onClicked: {
                        parent.isPressed = false
                        colorDialog.open()
                }
                onPressed: {
                        parent.isPressed = true
                }
                onReleased: {
                        parent.isPressed = false
                }
            }
            Item{id:centerItem; anchors.centerIn: parent}
            Rectangle{ id: colorPreviewDuplicate
                opacity: (!isAnyToolSelected || currentTool === "Erase")? (parent.enabled?1:0.5): 0
                color: currentTool === "Erase" ? "white" : parent.enabled ? currentToolColour ? currentToolColour : "grey" : "grey"
                border.width: 1
                border.color: parent.enabled? "black" : "dark grey"
                anchors.left: centerItem.horizontalCenter
                anchors.leftMargin: 18
                anchors.right: parent.right
                anchors.rightMargin: 3
                height: buttonHeight/1.4;
                anchors.verticalCenter: centerItem.verticalCenter
                onYChanged: {
                    colorPreview.x= colorPreviewDuplicate.x
                    colorPreview.y= colorPreviewDuplicate.y
                }
            }
            Rectangle{ id: colorPreview
                visible: (isAnyToolSelected && currentTool !== "Erase")
                x: colorPreviewDuplicate.x
                y: colorPreviewDuplicate.y
                width: colorPreviewDuplicate.width
                onWidthChanged: {
                    x= colorPreviewDuplicate.x
                    y= colorPreviewDuplicate.y
                }
                height: colorPreviewDuplicate.height
                color: colorPreviewDuplicate.color
                border.width: 1
                border.color: "black"
                scale: dragArea.drag.active? 0.7: 1

                Drag.active: dragArea.drag.active
                Drag.hotSpot.x: colorPreview.width/2
                Drag.hotSpot.y: colorPreview.height/2
                MouseArea{
                    id: dragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: parent
                    drag.minimumX: -toolProperties.width/2
                    drag.maximumX: toolProperties.width
                    drag.minimumY: buttonHeight
                    drag.maximumY: buttonHeight*4
                    onReleased: {
                        colorProp.isPressed = false
                        parent.Drag.drop()
                        parent.x = colorPreviewDuplicate.x
                        parent.y = colorPreviewDuplicate.y
                    }
                    onClicked: {
                        colorProp.isPressed = false
                        colorDialog.open()
                    }
                    onPressed: {
                        colorProp.isPressed = true
                    }
                }
            }
        }

    }

    XsColourPresetsLR{ id: row3_colourpresets
        x: framePadding
        y: row2_controls.y + row2_controls.height + itemSpacing*1.5
        onYChanged: {
            toolPropLoaderHeight = row3_colourpresets.y + row3_colourpresets.height
        }

        visible: (isAnyToolSelected && currentTool !== "Erase")
        width: toolProperties.width- framePadding*2
        height: visible? buttonHeight*2 + framePadding : 0
        onHeightChanged: {
            toolPropLoaderHeight = row3_colourpresets.y + row3_colourpresets.height
        }
    }


    Component.onCompleted: {
        toolPropLoaderHeight = row3_colourpresets.y + row3_colourpresets.height
        setPropertyIndeces()
    }

}

