// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import "./list_view"
import "./grid_view"
import "./widgets"
import "./functions"

Item {

    height: toolbar.height

    // Note: RowLayout has a 'hard' minimum width which sets the minimum
    // width of the parent layout (if there is one) - depending on the width of
    // the items in the RowLayout. I this case we don't want that, as it
    // makes the medialist itself have a minimum width causing the scrollbar
    // on the right to disappear if the view is made very narrow.
    // By Wrapping in an Item here we get around this as the RowLayout gets
    // clipped by the Item.

    RowLayout{
        id: toolbar

        spacing: 1
        anchors.left: parent.left
        anchors.right: parent.right

        XsPrimaryButton{ id: addBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            Layout.alignment: Qt.AlignLeft
            imgSrc: "qrc:/icons/add.svg"
            onClicked: togglePlusMenu(width/2, height/2, addBtn)
        }

        XsPrimaryButton{ id: deleteBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            imgSrc: "qrc:/icons/delete.svg"
            onClicked: toggleRemoveMenu(width/2, height/2, deleteBtn)
        }

        XsSearchButton{
            id: searchBtn
            Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
            Layout.preferredHeight: btnHeight
            isExpanded: false
            hint: "Search media..."
            onTextChanged: {
                mediaListSearchString = text
                if (mediaListSearchString != "" && !isExpanded) {
                    isExpanded = true
                } else if (!visible) {
                    isExpanded = false
                }
            }
            property var ss: mediaListSearchString
            onSsChanged: {
                if (ss != text) text = ss
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: btnHeight
        }

        // XsComboBox{ id: sortValueDiv
        //     Layout.minimumWidth: btnWidth
        //     Layout.preferredWidth: btnWidth*3
        //     Layout.maximumWidth: btnWidth*3
        //     Layout.fillWidth: true
        //     Layout.preferredHeight: btnHeight
        //     visible: !is_list_view
        //     onCurrentIndexChanged: {
        //     }
        // }
        // XsPrimaryButton{ id: sortOrderBtn
        //     Layout.preferredWidth: btnWidth/2
        //     Layout.preferredHeight: btnHeight
        //     imgSrc: "qrc:/icons/trending.svg"
        //     imageDiv.rotation: isAsc? 90:270
        //     visible: !is_list_view
        //     property bool isAsc: true
        //     onClicked: {
        //         sortOrderBtn.isAsc = !sortOrderBtn.isAsc
        //     }
        // }


        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: btnHeight
        }
        // XsText{
        //     Layout.fillWidth: true
        //     Layout.minimumWidth: 0//btnWidth
        //     Layout.preferredHeight: btnHeight
        //     text: searchBtn.isExpanded? "" : inspectedMediaSetProperties.values.nameRole ? inspectedMediaSetProperties.values.nameRole : ""
        //     font.bold: true
        //     elide: Text.ElideRight
        //     opacity: searchBtn.isExpanded? 0:1
        //     Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuart }  }
        // }

        XsIntegerValueControl {
            visible: !is_list_view

            Layout.minimumWidth: btnWidth
            Layout.preferredWidth: btnWidth*2
            Layout.maximumWidth: btnWidth*2
            Layout.preferredHeight: btnHeight
            Layout.maximumHeight: btnHeight

            text: "Scale"
            fromValue: 100
            toValue: 300
            defaultValue: 200
            stepSize: 1
            valueText: parseFloat(cellSize/100).toFixed(2)

            property real value: cellSize
            onValueChanged:{

                if(cellSize != value)
                    cellSize = value
            }

            Component.onCompleted: {
                if(user_data.cellSize)
                    cellSize = user_data.cellSize
            }
        }

        XsPrimaryButton{ id: gridViewBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            imgSrc: "qrc:/icons/view_grid.svg"
            isActive: !is_list_view
            onPressed: {
                is_list_view = false
            }
        }
        XsPrimaryButton{
            id: listViewBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            imgSrc: "qrc:/icons/list.svg"
            isActive: is_list_view
            onPressed: {
                is_list_view = true
            }
        }
        XsPrimaryButton{ id: moreBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            Layout.alignment: Qt.AlignRight
            imgSrc: "qrc:/icons/more_vert.svg"
            onClicked: toggleContextMenu(width/2, height/2, moreBtn)
        }
    }
}