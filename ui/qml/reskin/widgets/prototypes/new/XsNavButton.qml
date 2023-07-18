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
        Text {
            id: textDiv
            text: isShort? widget.shortTerm : widget.text
            font: widget.font
            color: textColorNormal 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 2
            bottomPadding: 2
            leftPadding: 20
            rightPadding: 20
            anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            width: parent.width
            height: parent.height

            XsToolTip{
                text: widget.text
                visible: widget.hovered && parent.truncated
                width: metricsDiv.width == 0? 0 : widget.width
                // x: 0 //#TODO: flex/pointer
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
        color: widget.down || widget.isActive? bgColorPressed : forcedBgColorNormal
        
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

