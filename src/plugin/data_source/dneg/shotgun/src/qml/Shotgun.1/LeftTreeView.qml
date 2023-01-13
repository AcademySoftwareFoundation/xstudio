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


Rectangle{

    color: "transparent" //palette.base
    border.color: frameColor
    border.width: frameWidth

    // property alias backendModel: treeView.delegateModel.model
    // property int mCount: sequenceTreeModel.count
    // onMCountChanged:{
    //     console.log("####R_m: "+sequenceTreeModel.count)
    //     // childLevel1.childViewHeight: 
    // }
    

    XsTreeStructure{ 
        id: treeView //level0
        x: frameWidth
        y: framePadding
        scrollBar.visible: !searchShotListPopup.visible && treeView.height < treeView.contentHeight

        delegateModel.model: sequenceTreeModel
        delegateModel.delegate: XsTreeStructureDelegate{

            id: childLevel1
            childViewHeight: 5*treeItemHeight
            childView.sourceComponent: XsTreeStructure{

                delegateModel.model: sequenceTreeModel //treeView.delegateModel.model 
                delegateModel.rootIndex: treeView.delegateModel.modelIndex(index, sequenceTreeModel.rootIndex) 
                delegateModel.delegate: XsTreeStructureDelegate{

                    // id: childLevel2
                    // childViewHeight: searchPresetsViewModel.length*treeItemHeight + treeItemHeight
                    // childView.sourceComponent: XsTreeStructure{ //Level-3
                    //     delegateModel.model: childLevel1.delegateModel.model 
                    //     delegateModel.rootIndex: childLevel1.delegateModel.modelIndex(index, childLevel1.delegateModel.model.rootIndex) 
                    //     delegateModel.delegate: XsTreeStructureDelegate{}
                    // }

                }

                // Component.onCompleted: { //#TODO: remove
                //     console.log("####R_c: "+sequenceTreeModel.count)
                //     console.log("####R_l: "+sequenceTreeModel.length)
                // }
                
            }

        }

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