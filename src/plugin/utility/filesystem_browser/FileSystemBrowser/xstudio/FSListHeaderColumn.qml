// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

Item{

    id: headerItem

    width: size

    property real thumbWidth: 1
    property bool containsMouse: dragIdx == index
    property bool isDragging: false

    XsText{ 
        anchors.fill: parent
        anchors.leftMargin: index ? 0 : 12
        id: titleDiv
        text: title ? title : ""
        horizontalAlignment: index ? Text.AlignHCenter : Text.AlignLeft
        elide: Text.ElideRight
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        hoverEnabled: true
    }

    XsSecondaryButton {
        width: height
        height: parent.height-6
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.topMargin: 3
        imgSrc: "qrc:/icons/arrow_upward.svg"
        rotation: scanResultsModel.sortAscending ? 180 : 0
        visible: ma.containsMouse || hovered || sortRole == sortId
        onClicked: {
            if (sortRole != sortId) {
                sortRole = sortId
            } else {
                sortAscending = !sortAscending
            }
        }
    }

        
    Rectangle{
        visible: index != 0
        x: containsMouse ? -1 : 0
        width: containsMouse ? 3 : 1
        height: parent.height
        color: containsMouse && dragging ? XsStyleSheet.accentColor : XsStyleSheet.widgetBgNormalColor
    }


}