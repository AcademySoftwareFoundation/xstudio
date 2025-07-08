// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

import xstudio.qml.bookmarks 1.0

import xStudio 1.0

GridLayout {

    columns: 4
    rows: 2
    columnSpacing: 0
    rowSpacing: 0
    Repeater {

        model: currentColorPresetModel
        Rectangle {

            Layout.fillWidth: true
            Layout.fillHeight: true
            color: modelData
            border.color: ma.containsMouse ? palette.highlight : "transparent"

            MouseArea{

                id: ma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    currentToolColour = color
                }
            }

        }
    }
}

