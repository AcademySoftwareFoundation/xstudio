// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects


import xstudio.qml.bookmarks 1.0



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

            }
        }
    }


}

