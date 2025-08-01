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

    anchors.fill: parent

    // note this model only has one item, which is the 'tool type' attribute
    // in the backend.

    XsModuleAttributesModel {
        id: annotations_tool_types
        attributesGroupNames: "annotations_tool_types_0"
    }

    property var toolImages: [
        "qrc:///icons/drawing.png",
        "qrc:///feather_icons/star.svg",
        "qrc:///feather_icons/type.svg",
        "qrc:///feather_icons/book.svg"
    ]

    // we have to use a repeater to hook the model into the ListView
    Repeater {

        id: the_view
        anchors.fill: parent
        anchors.margins: framePadding

        // by using annotations_tool_types as the 'model' we are exposing the
        // attributes in the "annotations_tool_types" group and their role.
        // The 'ListView' is instanced for each attribute, and each instance
        // can 'see' the attribute role data items (like 'value', 'combo_box_options').
        // In this case, there is only one attribute in the group which tracks
        // the 'active tool' selection for the annotations plugin.
        model: annotations_tool_types

        ListView{ 
            
            id: toolSelector

            anchors.fill: parent
            anchors.margins: framePadding

            spacing: itemSpacing
            // clip: true
            interactive: false
            orientation: ListView.Horizontal

            model: combo_box_options // this is 'role data' from the backend attr

            delegate: toolSelectorDelegate

            // read only convenience binding to backend.
            currentIndex: combo_box_options.indexOf(value)

            Component{ 
                
                id: toolSelectorDelegate


                Rectangle{
    
                    width: (toolSelector.width-toolSelector.spacing*(combo_box_options.length-1))/combo_box_options.length// - toolSelector.spacing
                    height: buttonHeight*2
                    color: "transparent"
                    property bool isEnabled: true//index != 2 // Text disabled while WIP - can be enabled to see where it is
                    enabled: isEnabled

                    XsButton{ id: toolBtn
                        width: parent.width
                        height: parent.height
                        text: ""
                        isActive: toolSelector.currentIndex===index
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        hoverEnabled: isEnabled

                        ToolTip {
                            parent: toolBtn
                            visible: !isEnabled && toolBtn.down
                            text: "Text captions coming soon!"
                        }

                        Text{
                            id: tText
                            text: combo_box_options[index]

                            font.pixelSize: fontSize
                            font.family: fontFamily
                            color: enabled? toolSelector.currentIndex===index || toolBtn.down || toolBtn.hovered || parent.isActive? toolActiveTextColor: toolInactiveTextColor : Qt.darker(toolInactiveTextColor,1.5)
                            horizontalAlignment: Text.AlignHCenter

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: framePadding/2
                        }
                        Image {
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: framePadding/2
                            width: 20
                            height: width
                            source: toolImages[index]
                            anchors.horizontalCenter: parent.horizontalCenter
                            layer {
                                enabled: true
                                effect:
                                ColorOverlay {
                                    color: enabled? (toolSelector.currentIndex===index || toolBtn.down || toolBtn.hovered)? toolActiveTextColor: toolInactiveTextColor : Qt.darker(toolInactiveTextColor,1.5)
                                }
                            }
                        }

                        onClicked: {
                            if (!isEnabled) return;
                            if(toolSelector.currentIndex == index)
                            { 
                                //Disables tool by setting the 'value' of the 'active tool'
                                // attribute in the plugin backend to 'None'
                                value = "None"
                            }
                            else
                            {
                                value = tText.text
                            }
                        }

                    }

                    // Rectangle {
                    //     anchors.fill: parent
                    //     visible: !isEnabled
                    //     color: "black"
                    //     opacity: 0.2
                    // }
                }
            }
        }
    }
}

