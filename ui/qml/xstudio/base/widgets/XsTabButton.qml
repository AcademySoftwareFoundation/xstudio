// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle

import xStudio 1.1

TabButton { id: widget


    property alias bgDiv: bgDiv
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: palette.base
    property color borderColorNormal: palette.base
    property real borderWidth: 1
    property real borderRadius: 2

    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"
    property var textElide: textDiv.elide
    property alias textDiv: textDiv

    property bool isActive: false
    property bool subtleActive: false


    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled ? 1.0: bgColorNormal!=palette.base?1: 0.3
        Text {
            id: textDiv
            text: widget.text
            // font: widget.font
            // font.bold: true
            font.weight: Font.DemiBold
            color: widget.down || widget.hovered? textColorPressed: textColorNormal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            elide: Text.ElideRight
            width: parent.width
            height: parent.height
        }
    }

    ToolTip.text: widget.text
    ToolTip.visible: widget.hovered && textDiv.truncated

    background: 
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        // opacity: enabled ? 1: 0.3
        color: enabled && widget.hoverEnabled? widget.down || (isActive && !subtleActive)? bgColorPressed: bgColorNormal : Qt.darker(bgColorNormal,1.5)
        border.color: widget.down || widget.hovered ? bgColorPressed: borderColorNormal
        border.width: borderWidth
        radius: borderRadius
    }

}