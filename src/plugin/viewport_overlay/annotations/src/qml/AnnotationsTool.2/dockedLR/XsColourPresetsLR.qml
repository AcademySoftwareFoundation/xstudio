// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

import xstudio.qml.bookmarks 1.0



import xStudio 1.0

Rectangle{

    color: "transparent"

    property int itemCount: currentColorPresetModel.count

    GridView{ id: presetColours
        x: 0
        width: parent.width - x*2
        height: buttonHeight*2
        clip: false
        interactive: false


        flow: GridView.FlowLeftToRight
        cellWidth: width/4
        cellHeight: buttonHeight

        model: currentColorPresetModel
        delegate:
        Item{
            property bool isMouseHovered: presetMArea.containsMouse
            width: presetColours.cellWidth 
            height: presetColours.cellHeight 

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                radius: 0
                color: preset

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

