// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQml.Models 2.14

import xStudio 1.1


Item { id: widget
    property alias model: treeModel.model
    property alias rootIndex: treeModel.rootIndex
    property var selectionModel: null
    property var expandedModel: null
    property var childObj: null

    property alias interactive: treeView.interactive
    property bool scrollBarVisibility: true
    signal shotClicked(string shot)

    x: frameWidth + framePadding
    // y: frameWidth //+ framePadding
    width: parent.width - x*2
    height: parent.height - y*2

    function createTreeNode(parent, myModel, myRootIndex, mySelectionModel, myExpandedModel) {
        var obj = Qt.createComponent("ShotsTreeView.qml").createObject(parent, 
            {model: myModel, rootIndex: myRootIndex, selectionModel: mySelectionModel, expandedModel: myExpandedModel,
                x: itemHeight/1.25-frameWidth*2, scrollBarVisibility:false} //, interactive: false}
            )
        if (obj == null) {
            console.log("Error creating ShotsTreeView object at "+myRootIndex);
        }
        else{
            childObj = obj
        }
    }

    // treeModel.model.insertRows(index, 1, treeModel.rootIndex)
    // treeModel.model.removeRows(index, 1, treeModel.rootIndex)
    // treeModel.model.moveRows(treeModel.rootIndex, index, 1, treeModel.rootIndex, index-1)
    // treeModel.model.moveRows(treeModel.rootIndex, index+1, 1, treeModel.rootIndex, index)
    // treeModel.model.moveRows(treeModel.rootIndex, index, 1, treeModel.parentModelIndex(), 0)


    ListView {
        id: treeView
        property bool isIconVisible: false
        
        model: treeModel

        anchors.fill: parent
        spacing: itemSpacing/2
        clip: true
        // snapMode: ListView.SnapToItem
        orientation: ListView.Vertical
        interactive: true
        currentIndex: -1
    
        ScrollBar.vertical: XsScrollBar {id: scrollBar; visible: scrollBarVisibility && treeView.height < treeView.contentHeight}
        
        DelegateModel { id: treeModel

            function remove(row) {
                model.removeRows(row, 1, rootIndex)
            }
        
            function move(src_row, dst_row) {
                // console.log("TreeDelegateModel.qml", src_row, "->", dst_row, rootIndex)
                // invert logic if moving down
                if(dst_row > src_row) {
                    model.moveRows(rootIndex, dst_row, 1, rootIndex, src_row)
                } else {
                    model.moveRows(rootIndex, src_row, 1, rootIndex, dst_row)
                }
            }
        
            function insert(row, data) {
                model.insert(row, rootIndex, data)
            }
        
            function append(data, root=rootIndex) {
                model.insert(rowCount(root), root, data)
            }
        
            function rowCount(root=rootIndex) {
                return model.rowCount(root)
            }
        
            function get(row, role) {
                return model.get(row, rootIndex, role)
            }
        
            function set(row, value, role) {
                return model.set(row, value, role, rootIndex)
            }

            
            model: null

            delegate:
            MouseArea { id: treeDelegate
                width: treeView.width
                height: isExpanded? (treeItemHeight + childView.height) : treeItemHeight

                // Rectangle{anchors.fill: parent; color:"red"; opacity: index%2==0?0.1:0.4; border.color:"green"}

                property bool isExpanded: expandedModel.selectedIndexes.includes(treeModel.modelIndex(index, treeModel.rootIndex, expandedModel))
                property bool isPopulated: false
                // property bool isSelected: treeView.currentIndex == index
                property bool isSelected: selectionModel.selectedIndexes.includes(treeModel.modelIndex(index, treeModel.rootIndex, selectionModel))
                property int childCount: treeModel.rowCount(treeModel.modelIndex(index)) //treeModel.rowCount(treeModel.modelIndex(index, treeModel.rootIndex))
                

                property bool isParent: typeRole=="Sequence" && childCount>0 && model.hasModelChildren
                property bool isMouseHovered: containsMouse
                
                property real treeItemHeight: itemHeight/1.25
                hoverEnabled: true
                // acceptedButtons: Qt.LeftButton | Qt.RightButton
                
                onClicked: {
                    // treeView.currentIndex = treeView.currentIndex == index ? -1 : index
                    selectionModel.select(treeModel.modelIndex(index, treeModel.rootIndex), ItemSelectionModel.ClearAndSelect)
                }
                onDoubleClicked: {
                    if(isParent) {
                        console.log("Seq1: "+nameRole)
                        shotClicked(nameRole+"")
                    }
                    else{
                        console.log("Shot1: "+nameRole)
                        shotClicked(nameRole+"")
                    }
                    
                }
                // onPressAndHold: {
                //     treeView.isIconVisible = !treeView.isIconVisible
                // }
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle{id: nameDiv
                        property bool divSelected: false
                        property int slNumber: index+1
            
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: treeItemHeight
                        color: "transparent"
                    
                        XsButton{id: expandButton
                        
                            text: ""
                            imgSrc: "qrc:/feather_icons/chevron-down.svg"
                            // visible: !isCollapsed && (isMouseHovered || hovered || isActive)
                            width: height
                            height: parent.height - frameWidth*2
                            image.sourceSize.width: height/1.2
                            image.sourceSize.height: height/1.2
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                            isActive: isExpanded
                            enabled: isParent
                        
                            scale: rotation==0 || rotation==-90? 1 : 0.85
                            rotation: (isExpanded)? 0: -90
                            Behavior on rotation {NumberAnimation{id: rotationAnim; duration: 150 }}
                        
                            onClicked: {
                                if(!isExpanded) {
                                    if(!isPopulated) {
                                        createTreeNode(childView, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex), selectionModel, expandedModel)
                                        isPopulated = true
                                    }
                                }
                                expandedModel.select(treeModel.modelIndex(index, treeModel.rootIndex), ItemSelectionModel.Toggle)
                            }
                            Component.onCompleted: {
                                if(isExpanded) {
                                    createTreeNode(childView, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex), selectionModel, expandedModel)
                                    isPopulated = true
                                }
                            }
                        }
                    
                        Image { id: icon
                            width: treeView.isIconVisible? height : 0
                            height: parent.height - framePadding
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: expandButton.right
                            anchors.leftMargin: treeView.isIconVisible? framePadding : 0
                            source: isParent?"qrc:/feather_icons/archive.svg":"qrc:/feather_icons/box.svg"
                            sourceSize.width: height/1.3
                            sourceSize.height: height/1.3
                            layer {
                                enabled: true
                                effect:
                                ColorOverlay {
                                    color: isMouseHovered? textColorNormal : isSelected? itemColorActive : "#404040"
                                }
                            }
                        }
                    
                        XsText{
                            text: !isCollapsed? nameRole : nameRole.substr(0,2)+".."
                            width: isCollapsed? parent.width: parent.width - expandButton.width - icon.width - framePadding*5
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: icon.right
                            anchors.leftMargin: framePadding
                        
                            elide: Text.ElideRight
                            font.pixelSize: fontSize*1.2
                            color: isSelected? itemColorActive : isMouseHovered? textColorActive : textColorNormal//itemColorNormal
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            ToolTip.text: nameRole
                            ToolTip.visible: isMouseHovered && isCollapsed
                            visible: !(treeView.isEditable && treeView.menuActionIndex == index)
                        }
            
                    }

                    Item { id: childView
            
                        // visible: isExpanded
                        Layout.fillWidth: true
                        Layout.minimumWidth: parent.width
                        Layout.fillHeight: true
                        Layout.minimumHeight: isExpanded? treeItemHeight*childCount : 0 //#TODO: Handle height when levels>2
                        // Rectangle{anchors.fill: parent; color:"blue"; opacity: index==2?0.1:0.4; border.color:"yellow"}

                    }

                }
            }
        }
    
    }

}
