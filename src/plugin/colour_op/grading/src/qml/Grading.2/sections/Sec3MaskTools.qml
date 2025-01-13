// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14

import xStudio 1.0
import Grading 2.0

import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.bookmarks 1.0

Item{

    property real maskBtnWidth: 100
    property string activeButton: "Polygon"
    property real itemHeight: XsStyleSheet.widgetStdHeight

    ListModel{ id: mask1ToolsModel
        ListElement{
            toolName: "Polygon"
            toolImg: "qrc:/grading_icons/hexagon.svg"
        }
        ListElement{
            toolName: "Ellipse"
            toolImg: "qrc:/icons/radio_button_unchecked.svg"
        }
    }
    ListModel{ id: mask2ToolsModel

        ListElement{
            toolName: "Dodge"
            toolImg: "qrc:/grading_icons/brightness_low.svg"
        }
        ListElement{
            toolName: "Burn"
            toolImg: "qrc:/grading_icons/brightness_high.svg"
        }
    }

    RowLayout {
        width: parent.width
        height: btnHeight
        anchors.centerIn: parent
        spacing: buttonSpacing
        
        RowLayout {
            spacing: buttonSpacing
            Layout.preferredWidth: maskBtnWidth*2 + spacing
            Layout.maximumWidth: maskBtnWidth*2 + spacing
            Layout.preferredHeight: btnHeight

            Repeater{
                model: mask1ToolsModel

                GTToolButtonHorz{
                    Layout.fillWidth: true
                    Layout.minimumWidth: maskBtnWidth/1.5
                    Layout.preferredWidth: maskBtnWidth
                    Layout.maximumWidth: maskBtnWidth
                    Layout.preferredHeight: btnHeight
                    txt: toolName
                    src: toolImg

                    Component.onCompleted: {
                        if (toolName == "Polygon") {
                            isActive = Qt.binding(function() { return mask_attrs.polygon_init })
                        }
                    }

                    onClicked: {
                        activeButton = toolName;
                        mask_attrs.mask_shapes_visible = true

                        if (toolName == "Polygon") {
                            mask_attrs.polygon_init = !mask_attrs.polygon_init;
                        } else if (toolName == "Ellipse") {
                            mask_attrs.drawing_action = "Add ellipse";
                        }
                    }
                }

            }

        }
        Item{
            // Layout.fillWidth: true
            Layout.minimumWidth: 2
            Layout.maximumWidth: 2
            Layout.fillHeight: true
        }

        // Dodge & Burn not implemented yet...
        // Repeater{
        //     model: mask2ToolsModel
        //     GTToolButton{ id: polyBtn
        //         Layout.fillWidth: true
        //         Layout.minimumWidth: maskBtnWidth/2
        //         Layout.maximumWidth: maskBtnWidth/2
        //         Layout.preferredHeight: itemHeight *2 + 1
        //         text: toolName
        //         src: toolImg
        //         isActive: activeButton == text
        //         enabled: false
        //         onClicked: {
        //             activeButton = text
        //         }
        //     }
        // }

        RowLayout{ id: propertiesGrid
            enabled: mask_attrs.mask_selected_shape >= 0
            spacing: buttonSpacing

            // Layout.fillWidth: true
            Layout.preferredWidth: opacityBtn.width*3 + removeBtn.width
            Layout.preferredHeight: btnHeight

            XsIntegerAttrControl { id: opacityBtn
                Layout.minimumWidth: maskBtnWidth/2
                Layout.preferredWidth: maskBtnWidth
                Layout.maximumWidth: maskBtnWidth
                Layout.fillHeight: true
                text: "Opacity"
                attr_group_model: mask_attrs.model
                attr_title: "Pen Opacity"
            }
            XsIntegerAttrControl {
                Layout.minimumWidth: maskBtnWidth/2
                Layout.preferredWidth: maskBtnWidth
                Layout.maximumWidth: maskBtnWidth
                Layout.fillHeight: true
                text: "Softness"
                attr_group_model: mask_attrs.model
                attr_title: "Pen Softness"
            }
            XsIntegerAttrControl {
                Layout.minimumWidth: maskBtnWidth/2
                Layout.preferredWidth: maskBtnWidth
                Layout.maximumWidth: maskBtnWidth
                Layout.fillHeight: true
                visible: activeButton == "Dodge" || activeButton == "Burn"
                text: "Size"
                attr_group_model: mask_attrs.model
                attr_title: "Shapes Pen Size"
            }
            XsPrimaryButton{ id: invertBtn
                Layout.fillWidth: true
                Layout.minimumWidth: maskBtnWidth/2
                Layout.preferredWidth: maskBtnWidth
                Layout.maximumWidth: maskBtnWidth
                Layout.fillHeight: true
                font.pixelSize: XsStyleSheet.fontSize
                text: "Invert"
                // imgSrc: "qrc:/grading_icons/invert_colors.svg"
                isActive: mask_attrs.shape_invert
                onClicked:{
                    mask_attrs.shape_invert = !mask_attrs.shape_invert
                }
            }
            Item{
                Layout.minimumWidth: 2
                Layout.maximumWidth: 2
                Layout.fillHeight: true
            }
            XsPrimaryButton{ id: removeBtn
                Layout.fillWidth: true
                Layout.minimumWidth: maskBtnWidth/4
                Layout.preferredWidth: maskBtnWidth/2
                Layout.maximumWidth: maskBtnWidth/2
                Layout.fillHeight: true
                font.pixelSize: XsStyleSheet.fontSize
                text: "Remove"
                imgSrc: "qrc:/icons/delete.svg"
                onClicked:{
                    mask_attrs.drawing_action = "Remove shape"
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.minimumWidth: 2
            // Layout.maximumWidth: 2
            Layout.fillHeight: true
        }

    }
}