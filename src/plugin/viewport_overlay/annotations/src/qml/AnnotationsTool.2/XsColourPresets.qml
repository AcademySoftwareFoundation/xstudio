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

Rectangle{

    color: "transparent"

    property int itemCount: currentColorPresetModel.count

    ListView{ id: presetColours
        x: framePadding
        width: parent.width - framePadding*2
        height: buttonHeight
        anchors.verticalCenter: parent.verticalCenter
        spacing: (itemSpacing!==0)?itemSpacing/2: 0
        clip: false
        interactive: false
        orientation: ListView.Horizontal

        model: currentColorPresetModel
        delegate:
        Item{
            property bool isMouseHovered: presetMArea.containsMouse
            width: index == itemCount-1? presetColours.width/itemCount : presetColours.width/itemCount-presetColours.spacing;
            height: presetColours.height
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                radius: 0
                color: preset
                border.width: 1
                border.color: parent.isMouseHovered? palette.highlight: (currentToolColour === preset)? "white": "black"

                MouseArea{
                    id: presetMArea
                    property color temp_color
                    anchors.fill: parent
                    hoverEnabled: true
                    smooth: true

                    onClicked: {
                        temp_color = currentColorPresetModel.get(index).preset;
                        currentToolColour = temp_color
                    }
                }

                DropArea {
                    anchors.fill: parent

                    Image {
                        visible: parent.containsDrag || presetMArea.containsMouse
                        anchors.centerIn: parent
                        width: parent.width<16? parent.width:16
                        height: width

                        source: parent.containsDrag? "qrc:///icons/add.svg" : "qrc:///anno_icons/draw_color_check.svg"
                        layer {
                            enabled: (preset=="black" || preset=="#000000") || presetMArea.pressed
                            effect:
                            ColorOverlay {
                                color: presetMArea.pressed? palette.highlight : "white"
                            }
                        }
                    }

                    onDropped: {
                        currentColorPresetModel.setProperty(index, "preset", currentToolColour.toString())
                    }
                }
            }
        }
    }


}

