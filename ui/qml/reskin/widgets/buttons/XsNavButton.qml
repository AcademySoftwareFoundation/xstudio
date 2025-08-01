// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget

    text: ""
    property bool isActive: false

    property color bgColorPressed: palette.highlight 
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property color textColorNormal: palette.text
    property var textElide: textDiv.elide
    property alias textDiv: textDiv
    property real textWidth: textDiv.textWidth

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    property bool isShort: false
    signal shortened()
    onShortened:{
        isShort = true
        console.log("NAV_btn_",text,": shortened")
    }
    signal expanded()
    onExpanded:{
        isShort = false
        console.log("NAV_btn_",text,": expanded")
    }

    contentItem:
    Item{
        anchors.fill: parent
        XsText {
            id: textDiv
            text: isShort? widget.shortTerm : widget.text
            font: widget.font
            color: textColorNormal 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 2
            bottomPadding: 2
            leftPadding: 5 //20
            rightPadding: 5 //20
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: parent.height

            XsToolTip{
                text: widget.text
                visible: widget.hovered && parent.truncated
                width: metricsDiv.width == 0? 0 : textWidth+22
                // x: 0 //#TODO: flex/pointer
            }
        }
    }

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: widget.hovered ? borderColorHovered: borderColorNormal
        border.width:  widget.hovered ? borderWidth : 0
        color: widget.down? bgColorPressed : forcedBgColorNormal
        
        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: false //widget.activeFocus //#TODO
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth 
            anchors.centerIn: parent
        }
        Rectangle{
            id: activeIndicator
            anchors.bottom: parent.bottom
            width: widget.width-(7*2) //textWidth+(7*2); 
            height: 2
            anchors.horizontalCenter: parent.horizontalCenter
            color: isActive? palette.highlight : "transparent"
        }
    }

    /*onPressed: focus = true
    onReleased: focus = false*/

}

