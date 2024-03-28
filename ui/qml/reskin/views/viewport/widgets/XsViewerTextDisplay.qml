// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import Qt.labs.qmlmodels 1.0
// import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle {
    id: widget
    color: isActive? Qt.darker(palette.highlight,2) : palette.base
    border.color: isHovered? palette.highlight : "transparent"

    property alias text: textDiv.text
    property color textColor: palette.highlight
    property bool isHovered: mArea.containsMouse
    property bool isActive: btnMenu.visible
    
    property alias modelDataName: btnMenuModel.modelDataName
    property alias menuWidth: btnMenu.menuWidth 

    XsText {
        id: textDiv
        text: ""
        color: textColor
        width: parent.width
        anchors.centerIn: parent
        tooltipVisibility: isHovered && textDiv.truncated
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

    XsMenuNew { 
        id: btnMenu
        visible: false
        menu_model: btnMenuModel
        menu_model_index: btnMenuModel.index(0, 0, btnMenuModel.index(-1, -1))
    }
    XsMenusModel {
        id: btnMenuModel
        modelDataName: ""
        onJsonChanged: {
            btnMenu.menu_model_index = btnMenuModel.index(0, 0, btnMenuModel.index(-1, -1))
        }
    }
}