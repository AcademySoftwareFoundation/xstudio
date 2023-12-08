// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget

    property string imgSrc: ""
    property bool isActive: false
 
    property color imgOverlayColor: "#C1C1C1"
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled ? 1.0 : 0.33
        Image {
            id: imageDiv
            source: imgSrc
            // width: parent.height-4
            // height: parent.height-4
            // topPadding: 2
            // bottomPadding: 2
            // leftPadding: 8
            // rightPadding: 8
            sourceSize.height: 16
            sourceSize.width: 16
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
            anchors.centerIn: parent
            smooth: true
            antialiasing: true
            layer {
                enabled: true
                effect: ColorOverlay { color: imgOverlayColor }
            }
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

    onPressed: focus = true
    onReleased: focus = false

}

