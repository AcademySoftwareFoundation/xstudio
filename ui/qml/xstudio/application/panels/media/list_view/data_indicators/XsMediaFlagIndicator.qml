// SPDX-License-Identifier: Apache-2.0
import QtQuick
import xStudio 1.0

Item {
    Rectangle{
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
        color: flagColourRole == undefined ? "transparent" : flagColourRole
    }

    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: headerThumbColor
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        acceptedModifiers: Qt.NoModifier
        onSingleTapped: (eventPoint, button)=> {
            showFlagMenu(eventPoint.position.x, eventPoint.position.y, parent, modelIndex())
        }
    }
}
