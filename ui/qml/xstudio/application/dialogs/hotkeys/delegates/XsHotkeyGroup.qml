// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQml.Models
import Qt.labs.qmlmodels
import QtQuick.Controls

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

Item {

    id: hotkeysListView
    height: mainRect.height + categoryLabel.height/2
    property var model
    property var rootIndex

    Rectangle {

        id: mainRect
        y: categoryLabel.height/2
        width: parent.width
        height: layout.height+borderSize*2
        property int borderSize: 16
        color: XsStyleSheet.baseColor
        border.color: XsStyleSheet.menuBorderColor
        border.width: 1
        
        GridLayout {

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: mainRect.borderSize
            anchors.top: parent.top
            id: layout
            clip: true
            columns: 1
            columnSpacing: 20
            rowSpacing: 0

            Repeater {
                model: DelegateModel {
                    model: hotkeysListView.model      
                    rootIndex: hotkeysListView.rootIndex
                    delegate: XsHotkeyDetails {
                        Layout.fillWidth: true
                    }
                    
                }
            }

        }
    }

    Rectangle {
        id: categoryLabel
        x: 14
        color: XsStyleSheet.panelBgColor
        border.color: XsStyleSheet.menuBorderColor
        border.width: 1
        height: label.height + 6
        width: label.width + 8
        XsLabel {
            id: label
            anchors.centerIn: parent
            text: hotkeyCategory
        }
    }
}

