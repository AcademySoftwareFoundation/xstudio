// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget

    property alias imgSrc: imageDiv.source
    property bool isActive: false
    property bool onlyVisualyEnabled: false
 
    property color imgOverlayColor: "#C1C1C1"
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property alias toolTip: toolTip

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true
    smooth: true
    antialiasing: true

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled || onlyVisualyEnabled ? 1.0 : 0.33
        XsImage {
            id: imageDiv
            sourceSize.height: 16
            sourceSize.width: 16
            imgOverlayColor: widget.imgOverlayColor
            anchors.centerIn: parent
        }
    }

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: widget.down || widget.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: widget.down || isActive? bgColorPressed : forcedBgColorNormal

        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: widget.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
    }


    XsToolTip{
        id: toolTip
        text: widget.text
        visible: false
        width: visible? text.width : 0 //widget.width
        x: 0 //#TODO: flex/pointer
    }

    onPressed: focus = true
    onReleased: focus = false

}

