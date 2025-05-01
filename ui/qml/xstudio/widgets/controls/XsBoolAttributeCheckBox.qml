// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Templates 2.12 as T
import QtQuick.Effects

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
    color: mouseArea.containsMouse ? XsStyleSheet.accentColor : "transparent"
    property string text
    property bool checked: value ? value : false
    property bool checkable: true
    property int iconsize: XsStyleSheet.menuHeight *.66
    z: 1000


    Rectangle {

        anchors.fill: parent
        border.width: 1 // mycheckableDecorated?1:0
        border.color: mouseArea.containsMouse ? XsStyleSheet.accentColor : XsStyleSheet.baseColor
        visible: checkable

        XsIcon {
            id: checkIcon
            visible: menu_item.checked
            source: "qrc:/icons/check.svg"
            width: iconsize // 2
            height: iconsize // 2
            anchors.centerIn: parent
            imgOverlayColor: textColor
        }

    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            value = !value
        }
    }

}
