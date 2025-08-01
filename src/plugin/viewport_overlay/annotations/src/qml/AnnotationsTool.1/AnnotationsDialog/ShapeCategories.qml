// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0

Item{ 
                    
    id: shapeCategories
    anchors.fill: parent
    visible: (currentTool === 3)
    property alias shapesModel: shapesModel

    // Rectangle{ id: divLine
    //     anchors.centerIn: parent
    //     width: parent.width - height*2
    //     height: frameWidth
    //     color: frameColor
    //     opacity: frameOpacity
    // }

    XsModuleAttributes {
        id: anno_tool_backend_settings
        attributesGroupNames: "annotations_tool_settings_0"
    }

    // make a local binding to the backend attribute
    property int shapeTool: anno_tool_backend_settings.shape_tool ? anno_tool_backend_settings.shape_tool : 0.0

    onShapeToolChanged: {
        shapesList.currentIndex = shapeTool
    }

    ListView{ id: shapesList

        width: parent.width-framePadding_x2
        height: buttonHeight
        x: framePadding + spacing/2
        // y: -parent.height/3
        anchors.verticalCenter: parent.verticalCenter

        spacing: itemSpacing
        clip: true
        interactive: false
        orientation: ListView.Horizontal

        model: shapesModel

        onCurrentIndexChanged: {
            if (anno_tool_backend_settings.shape_tool != undefined) {
                anno_tool_backend_settings.shape_tool = currentIndex
            }
        }

        delegate:
        XsButton{
            id: shapeBtn
            text: ""
            width: shapesList.width/shapesModel.count-shapesList.spacing;
            height: shapesList.height
            isActive: shapesList.currentIndex===index
            Image {
                anchors.centerIn: parent
                width: height
                height: parent.height-(shapesList.spacing*2)
                source: shapeImage
                layer {
                    enabled: true
                    effect:
                    ColorOverlay {
                        color: (shapesList.currentIndex===index || shapeBtn.hovered)? toolActiveTextColor: toolInactiveTextColor
                    }
                }
            }
            onClicked: {
                shapesList.currentIndex = index
            }
        }
    }

    ListModel{ id: shapesModel

        ListElement{
            shapeImage: "qrc:///feather_icons/square.svg"
            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" ><rect x=\"6\" y=\"6\" width=\"36\" height=\"36\" rx=\"4\" ry=\"4\"></rect></svg>"
        }
        ListElement{
            shapeImage: "qrc:///feather_icons/circle.svg"
            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" ><circle cx=\"24\" cy=\"24\" r=\"20\"></circle></svg>"
        }
        ListElement{
            shapeImage: "qrc:///feather_icons/arrow-right.svg"
            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"10\" y1=\"24\" x2=\"38\" y2=\"24\"></line><polyline points=\"24 10 38 24 24 38\"></polyline></svg>"
        }
        ListElement{
            shapeImage: "qrc:///feather_icons/minus.svg"
            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"10\" y1=\"24\" x2=\"38\" y2=\"24\"></line></svg>"
        }

    }

    function shapePreviewSVGSource(tool_size) {
        return shapesModel.get(shapesList.currentIndex).shapeSVG1 + (tool_size*7/100) + shapesModel.get(shapesList.currentIndex).shapeSVG2
    }

}
