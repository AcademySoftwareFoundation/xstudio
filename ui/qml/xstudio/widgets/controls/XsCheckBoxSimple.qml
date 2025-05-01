// SPDX-License-Identifier: Apache-2.0
import QtQuick 
import QtQuick.Window 
import QtQuick.Controls.Basic
import QtQuick.Effects

import xStudio 1.0

Item { 
    
    id: checkbox
    property bool checked: false
    property bool hovered: ma.containsMouse
    property bool pressed: ma.pressed

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            checkbox.clicked()
        }
    }

    signal clicked()

    XsGradientRectangle{
        id: bgDiv
        anchors.fill: parent

        flatColor: topColor
        topColor: checkbox.pressed ? palette.highlight: XsStyleSheet.controlColour
        bottomColor: checkbox.pressed? palette.highlight: XsStyleSheet.widgetBgNormalColor
    }

    Rectangle { 

        color: "transparent"
        border.color: checkbox.hovered ? palette.highlight : "transparent"
        border.width: 1
        anchors.fill: parent

    }

    XsIcon {
        anchors.fill: parent
        anchors.margins: 1
        visible: checkbox.checked
        source: "qrc:/icons/check.svg"
    }


}