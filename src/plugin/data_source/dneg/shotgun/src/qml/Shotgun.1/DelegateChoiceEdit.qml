// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import Qt.labs.qmlmodels 1.0

import xStudio 1.1

DelegateChoice {
    roleValue: "Edit"
    Rectangle {
        property bool isSelected: selectionModel.isSelected(searchResultsViewModel.index(index, 0))
        property bool isMouseHovered: mArea.containsMouse
        width: searchResultsView.cellWidth-itemSpacing
        height: searchResultsView.cellHeight-itemSpacing
        color: isSelected? Qt.darker(itemColorActive, 2.75): itemColorNormal
        border.color: isMouseHovered? itemColorActive: itemColorNormal
        clip: true

        Connections {
            target: selectionModel
            function onSelectionChanged(selected, deselected) {
                isSelected = selectionModel.isSelected(searchResultsViewModel.index(index, 0))
            }
        }

        MouseArea{
            id: mArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                searchResultsDiv.itemClicked(mouse, index, isSelected)
            }

        }

        Item{
            width: parent.width; height: parent.height

            Text{ id: txt1
                text: nameRole
                font.pixelSize: fontSize*1.2
                font.family: fontFamily
                font.bold: true
                color: isMouseHovered? textColorActive: textColorNormal
                width: parent.width
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                anchors.centerIn: parent
            }
        }
    }
}