// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Slider{ id: widget
    from: 0
    to: 10
    value: from

    stepSize: 0
    snapMode: Slider.SnapAlways
    // orientation: Qt.Vertical

    focusPolicy: Qt.WheelFocus
    wheelEnabled: true
    topPadding: 0

    property bool isHorizontal: orientation==Qt.Horizontal
    property color fillColor: palette.highlight
    property color handleColor: palette.text

    signal released()
    onPressedChanged : {
        if (pressed === false) {
            released()
        }
    }

    background: Rectangle {
        x: isHorizontal? 
            widget.leftPadding : 
            widget.leftPadding + widget.availableWidth/2 - width/2
        y: isHorizontal? 
            widget.topPadding + widget.availableHeight/2 - height/2 :
            widget.topPadding
        implicitWidth: isHorizontal? 250:6
        implicitHeight: isHorizontal? 6:250
        width: isHorizontal? widget.availableWidth : implicitWidth
        height: isHorizontal? implicitHeight : widget.availableHeight
        radius: implicitHeight/2
        color: fillColor 
        // rotation: isHorizontal? -90 : 0

        Rectangle {
            width: isHorizontal? widget.visualPosition*parent.width : parent.width
            height: isHorizontal?  parent.height : widget.visualPosition*parent.height
            color: palette.base
            radius: parent.radius
        }
    }

    handle: Rectangle {
        x: isHorizontal?
            widget.leftPadding + widget.visualPosition * (widget.availableWidth - width) :
            widget.leftPadding + widget.availableWidth / 2 - width / 2
        y: isHorizontal?
            widget.topPadding + widget.availableHeight / 2 - height / 2 :
            widget.topPadding + widget.visualPosition * (widget.availableHeight - height)
        implicitWidth: 16
        implicitHeight: 6
        // radius: implicitHeight/2
        color: widget.pressed ? Qt.darker(handleColor,1.3) : handleColor 
        border.color: widget.hovered? palette.highlight : "transparent"
    }

}