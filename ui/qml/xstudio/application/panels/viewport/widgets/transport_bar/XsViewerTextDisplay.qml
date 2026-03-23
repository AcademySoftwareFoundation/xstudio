// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0

Rectangle {
    property var modelDataName
    property string text: ""
    property alias textWidth: textDiv.textWidth
    property alias menuWidth: btnMenu.menuWidth

    color: btnMenu.visible ? Qt.darker(XsStyleSheet.accentColor, 2) : XsStyleSheet.panelBgColor
    border.color: mArea.containsMouse ? XsStyleSheet.accentColor : "transparent"

    XsText {
        id: textDiv
        text: parent.text
        color: XsStyleSheet.accentColor
        width: parent.width
        anchors.centerIn: parent
        font.family: XsStyleSheet.fixedWidthFontFamily
        font.pixelSize: 14

        MouseArea{
            id: mm
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true
        }

        XsToolTip {
            text: parent.text
            visible: mm.containsMouse && parent.truncated
            x: 0 //#TODO: flex/pointer
        }
    }

    MouseArea{
        id: mArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            btnMenu.x = x //-btnMenu.width
            btnMenu.y = y-btnMenu.height
            btnMenu.visible = !btnMenu.visible
        }
    }

    XsPopupMenu {
        id: btnMenu
        visible: false
        menu_model_name: modelDataName
    }

}