// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

Control {
    id: widget

    property alias text: displayText.text
    property alias textDiv: displayText
    property alias mouseDiv: mouseArea


    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"
    property var fontFamily: XsStyle.fontFamily
    property real fontSize: XsStyle.menuFontSize

    property bool forcedMouseHover: false
    property bool borderVisible: false
    property color bgColorNormal: "transparent" //palette.base
    property color borderColorPressed: palette.highlight
    property color borderColorNormal: Qt.darker(palette.base, 0.7) //palette.base
    property real borderWidth: 1
    property real borderRadius: 2

    property bool isClickable: true

    signal textClicked()
    implicitWidth: 50
    implicitHeight: 22*1.2

    
    MouseArea{
        id: mouseArea
        hoverEnabled: enabled
        cursorShape: widget.isClickable? Qt.PointingHandCursor : Qt.ArrowCursor 
        anchors.fill: widget
        propagateComposedEvents: true
        Rectangle{
            visible: bgColorNormal != "transparent"
            anchors.fill: parent; 
            color: bgColorNormal
            radius: borderRadius
            border.width: borderWidth
            border.color: borderVisible? borderColorNormal: "transparent"
        }
        onClicked: {
            if(widget.isClickable) textClicked()
            mouse.accepted = false
        }
    }
    
    XsText{ id: displayText
        color: widget.isClickable && mouseArea.pressed? borderColorPressed: (widget.hovered && widget.isClickable) || forcedMouseHover? textColorPressed: textColorNormal
        width: widget.width
        anchors.verticalCenter: widget.verticalCenter
        font.family: fontFamily
        font.pixelSize: fontSize
        // font.italic: false
        // font.bold: true
        font.underline: widget.hovered && widget.isClickable
    }

}



