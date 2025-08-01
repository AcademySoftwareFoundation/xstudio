// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Templates 2.12 as T
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

    // color: XsStyle.controlTitleColor
    // font {
    //     pixelSize: XsStyle.popupControlFontSize
    //     family: XsStyle.controlContentFontFamily
    //     hintingPreference: Font.PreferNoHinting
    // }

Rectangle {

    id: menu_item
    width: height
    height: iconsize
    color: mouseArea.containsMouse ? XsStyle.highlightColor : "transparent"
    property string text
    property bool checked: value ? value : false
    property bool checkable: true
    property int iconsize: XsStyle.menuItemHeight *.66
    gradient: mouseArea.containsMouse ? styleGradient.accent_gradient : null
    z: 1000
    

    Rectangle {

        anchors.fill: parent
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

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            console.log("clicked", value)
            value = !value
        }
    }

    property string tooltip_text: ""

    ToolTip.delay: 500
    ToolTip.visible: mouseArea.containsMouse && tooltip_text != ""
    ToolTip.text: tooltip_text


}
