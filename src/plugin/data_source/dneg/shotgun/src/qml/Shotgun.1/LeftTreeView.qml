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
import QuickFuture 1.0
import QuickPromise 1.0

import Shotgun 1.0
import xStudio 1.1
import xstudio.qml.clipboard 1.0


Rectangle{ id: section
    property var model: sequenceTreeModel
    color: "transparent" //palette.base
    border.color: frameColor
    border.width: frameWidth

    signal clicked(string type, string name, int id)
    signal doubleClicked(string type, string name, int id)


    XsTimer {
        id: callback_delay_timer
    }

    function selectItem(index) {
        let i = index.parent
        while(i.valid) {
            itemExpandedModel.select(i, ItemSelectionModel.Select)
            i = i.parent
        }
        callback_delay_timer.setTimeout(function(){ itemSelectionModel.select(index, ItemSelectionModel.ClearAndSelect) }, 100);
    }

    function scrollToFunc(target) {
        let ty = treeView.listView.mapFromItem(target, 0, 0).y + treeView.listView.contentY
        let visibleHeight = treeView.listView.visibleArea.heightRatio * treeView.listView.contentHeight

        if(ty < treeView.listView.contentY) {
            treeView.listView.contentY = ty
        } else if(ty > (treeView.listView.contentY + visibleHeight - 20) ){
            treeView.listView.contentY = treeView.listView.contentY + (ty - (treeView.listView.contentY + visibleHeight - 20))
        }
    }

    ItemSelectionModel {id: itemSelectionModel
        model: section.model
        onSelectionChanged: {
            if(selectedIndexes.length){
                clicked(
                    section.model.get(selectedIndexes[0],"typeRole"),
                    section.model.get(selectedIndexes[0],"nameRole"),
                    section.model.get(selectedIndexes[0],"idRole"),
                )
            }
        }
    }

    ItemSelectionModel {id: itemExpandedModel
        model: section.model
    }

    ShotsTreeView{ id: treeView
        model: section.model
        rootIndex:  null
        selectionModel: itemSelectionModel
        expandedModel: itemExpandedModel
        scrollBarVisibility: !searchShotListPopup.visible
        itemDoubleClicked: itemDoubleClickedFunction
        scrollTo: scrollToFunc
    }

    function itemDoubleClickedFunction(type, name, id) {
        doubleClicked(type, name, id)
    }

    Rectangle { id: overlapGradient
        visible: searchShotListPopup.visible
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#77000000" }
            GradientStop { position: 1.0; color: "#55000000" }
        }
        MouseArea{anchors.fill: parent;onClicked: searchTextField.focus = false; hoverEnabled: true}
    }
}