// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.0

import xstudio.qml.helpers 1.0

Rectangle {

    id: header
    color: XsStyle.mainBackground
    height: 20
    // z: 1000
    property var item_height: thumb_size*9/16
    property var thumb_size: min_thumb_size
    property var selection_indicator_width: 20
    property var filename_left: thumb_size + selection_indicator_width

    property int media_list_idx: 0
    property string row_height_property_name: "pane" + media_list_idx + "_row_height"
    property var parent_window_name: "main_window"
    property var min_thumb_size: 20

    Rectangle {

        color: XsStyle.mainBackground
        x: filename_left
        width: parent.width-filename_left
        //x: index ? repeater.itemAt(index-1).x+repeater.itemAt(index-1).width : 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        id: filnameHeader
        Text {
            color: "white"
            anchors.fill: parent
            anchors.margins: 8
            text: "File"
            verticalAlignment: Text.AlignVCenter
        }

    }

    Rectangle {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: selection_indicator_width - width/2
        width: 2
        color: "black"
    }

    Rectangle {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: filename_left - width/2
        width: 2
        color: underMouse ? "white" : "black"
        property bool underMouse: dragArea.underMouseIdx == 1

    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: underMouseIdx == 1 ? Qt.SizeHorCursor : Qt.ArrowCursor
        property real drag_start_x
        property var drag_start_thumb_size
        property int underMouseIdx: 0

        onPressed: {
            if (underMouseIdx != 0) {
                drag_start_x = mouseX
                drag_start_thumb_size = thumb_size
            }
        }

        onMouseXChanged: {
            if (pressed && underMouseIdx == 1) {
                var new_size = drag_start_thumb_size + (mouseX-drag_start_x)
                thumb_size = new_size < min_thumb_size ? min_thumb_size : new_size
            } else if (!pressed) {
                var divider_pos = thumb_size + selection_indicator_width
                if (Math.abs(mouseX-divider_pos) < 5) {
                    underMouseIdx = 1
                } else {
                    underMouseIdx = 0
                }
            }
        }
    }

}