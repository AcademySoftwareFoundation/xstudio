// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2
import QtQuick 2.14
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

Rectangle {

    id: menu_item
    width: ll.width+16
    height: ll.height+8
    color: mouseArea.containsMouse ? XsStyle.highlightColor : "transparent"
    property string text
    property bool checked: false
    property bool checkable: true
    property int iconsize: XsStyle.menuItemHeight *.66
    gradient: mouseArea.containsMouse ? styleGradient.accent_gradient : null
    z: 1000
    signal triggered    

    RowLayout {

        id: ll 
        spacing: 4
        y: 4
        x: 8

        Rectangle {
            width: iconsize
            height: iconsize
            Layout.margins: 0
            gradient: checked ? styleGradient.accent_gradient : null
            color: checked ? XsStyle.highlightColor : "transparent"
            border.width: 1 // mycheckableDecorated?1:0
            border.color: mouseArea.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
            visible: checkable

            Image {
                id: checkIcon
                visible: menu_item.checked
                sourceSize.height: XsStyle.menuItemHeight
                sourceSize.width: XsStyle.menuItemHeight
                source: "qrc:///feather_icons/check.svg"
                width: iconsize // 2
                height: iconsize // 2
                anchors.centerIn: parent
            }

            ColorOverlay{
                id: colorolay
                anchors.fill: checkIcon
                source:checkIcon
                visible: menu_item.checked
                color: XsStyle.hoverColor//menuItem.highlighted?XsStyle.hoverColor:XsStyle.menuBackground
                antialiasing: true
            }
        }

        Text {

            id: messageText
            text: menu_item.text
            Layout.margins: 0
            Layout.fillWidth: true

            font.pixelSize: XsStyle.popupControlFontSize
            font.family: XsStyle.controlTitleFontFamily
            color: XsStyle.hoverColor
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
    
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            menu_item.triggered()
        }
    }
}
