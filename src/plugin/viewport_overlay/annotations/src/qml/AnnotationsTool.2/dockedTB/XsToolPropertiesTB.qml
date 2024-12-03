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
        width: visible? row2_controls.gridItemWidth*1 : 0
        height: XsStyleSheet.primaryButtonStdHeight
        anchors.verticalCenter: parent.verticalCenter

        color: "transparent"
        visible: (currentTool == "Text")

        XsTextCategoriesTB {
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
        x: row1_categories.visible? row1_categories.x + row1_categories.width : row1_categories.x
        z: 1

        columns: currentTool === "Text"? 4 : currentTool === "Erase"? 2 : 3
        spacing: itemSpacing
        flow: Grid.TopToBottom
        width: gridItemWidth*columns
        height: XsStyleSheet.primaryButtonStdHeight
        anchors.verticalCenter: parent.verticalCenter

        property real gridItemWidth: 80 //toolPropLoaderWidth/(columns+1) > 80 ? 80 : toolPropLoaderWidth/(columns+1)

        XsIntegerAttrControl {
            id: sizeProp
            visible: enabled
            width: row2_controls.gridItemWidth
            height: visible? XsStyleSheet.primaryButtonStdHeight : 0
            text: (currentTool=="Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")? "Width" : "Size"
            enabled: isAnyToolSelected
            attr_group_model: annotations_model_data
            attr_title: toolSizeAttrName
        }

        XsIntegerAttrControl{
            id: opacityProp
            visible: isAnyToolSelected
            width: row2_controls.gridItemWidth
            height: visible? XsStyleSheet.primaryButtonStdHeight : 0
            text: "Opacity"
            attr_group_model: annotations_model_data
            attr_title: "Pen Opacity"
            enabled: isAnyToolSelected && currentTool != "Erase"
        }
        XsIntegerAttrControl {
            id: bgOpacityProp
            visible: currentTool === "Text"
            width: row2_controls.gridItemWidth
            height: visible? XsStyleSheet.primaryButtonStdHeight : 0
            text: "BG Opa."
            attr_group_model: annotations_model_data
            attr_title: "Text Background Opacity"
        }

        XsViewerMenuButton{ id: colorProp
            visible: isAnyToolSelected && currentTool !== "Erase"
            width: visible? row2_controls.gridItemWidth : 0
            height: XsStyleSheet.primaryButtonStdHeight
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
                anchors.left: centerItem.right
                anchors.leftMargin: 15
                anchors.right: parent.right
                anchors.rightMargin: 4
                height: XsStyleSheet.primaryButtonStdHeight/1.6;
                anchors.verticalCenter: centerItem.verticalCenter
                onXChanged: {
                    colorPreview.x= colorPreviewDuplicate.x
                    colorPreview.y= colorPreviewDuplicate.y
                }
            }
            Rectangle{ id: colorPreview
                visible: (isAnyToolSelected && currentTool !== "Erase")
                x: colorPreviewDuplicate.x
                y: colorPreviewDuplicate.y
                z: 100
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
                Drag.hotSpot.x: colorPreview.width
                Drag.hotSpot.y: colorPreview.height

                MouseArea{
                    id: dragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: parent
                    drag.minimumX: -toolProperties.width
                    drag.maximumX: toolProperties.width
                    drag.minimumY: XsStyleSheet.primaryButtonStdHeight
                    drag.maximumY: XsStyleSheet.primaryButtonStdHeight*4
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

    XsColourPresetsTB{ id: row3_colourpresets
        visible: (isAnyToolSelected && currentTool !== "Erase")

        x: row2_controls.x + row2_controls.width + itemSpacing*2
        onXChanged: {
            toolPropLoaderWidth = x + width
        }
        y: framePadding
        width: visible? row2_controls.gridItemWidth : 0
        height: parent.height - y*2
        onWidthChanged: {
            toolPropLoaderWidth = x + width
        }
    }



    Component.onCompleted: {
        toolPropLoaderWidth = row3_colourpresets.x + row3_colourpresets.width
        setPropertyIndeces()
    }

}

