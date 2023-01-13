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

import xStudio 1.1
import xstudio.qml.clipboard 1.0

MouseArea {
    id: dragArea
    property var model: DelegateModel.model
    property bool isOpened: false
    property bool isSelected: treeView.currentIndex == index
    property bool isParent: childView.sourceComponent
    property bool isIconVisible: treeView.isIconVisible
    property bool isMouseHovered: containsMouse
    property bool held: false
    property bool was_current: false
    property real treeItemHeight: itemHeight/1.25
    property alias childView: childView
    property real childViewHeight: 0
    
    hoverEnabled: true
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    anchors { 
        left: parent ? parent.left : undefined; 
        leftMargin: parent ? framePadding*2 : 0
        right: parent ? parent.right : undefined
    }
    height: treeNode.height

    // onClicked: {
    //     if(!expanded) {
    //         expanded = true
    //         if(!populated) {
    //             createTreeNode(children, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex))
    //             populated = true
    //         }
    //     } else {
    //         expanded = false
    //     }
    // }

    onClicked: {
        if(!held) {
            treeView.currentIndex = treeView.currentIndex == index ? -1 : index
        }
    }
    onDoubleClicked: {
        if(!held && isParent) {
            isOpened = !isOpened
        }
    }
    onPressAndHold: {
        // held = true
        treeView.isIconVisible = !treeView.isIconVisible
    }
    // onReleased: {
    //     held = false
    // }

    Rectangle {
        id: treeNode
        color: "transparent"
        width: dragArea.width
        height: (treeItemHeight + childView.height)
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
    
        Drag.active: dragArea.held
        Drag.source: dragArea
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
    
        states: State {
            when: dragArea.held
        
            ParentChange { target: treeNode; parent: presetsDiv }
            AnchorChanges {
                target: treeNode
                anchors { horizontalCenter: undefined; verticalCenter: undefined }
            }
        }
    
        Rectangle{id: nameDiv
            property bool divSelected: false
            property int slNumber: index+1

            width: parent.width
            height: treeItemHeight
            color: "transparent"
            
            anchors {
                right: parent.right
                top: parent.top
            }
        
            XsButton{id: expandButton
                property bool isExpanded: isOpened
            
                text: ""
                imgSrc: "qrc:/feather_icons/chevron-down.svg"
                // visible: !isCollapsed && (isMouseHovered || hovered || isActive)
                width: height
                height: parent.height - frameWidth
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
                    isOpened = !isOpened
                }
            }
        
            Image { id: icon
                width: isIconVisible? height : 0
                height: parent.height - framePadding
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: expandButton.right
                anchors.leftMargin: isIconVisible? framePadding : 0
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
    
        // Child
        Loader {
            id: childView
        
            visible: isOpened
            x: frameWidth + framePadding//*2
            y: treeItemHeight + frameWidth
            width: parent.width - frameWidth*2 -framePadding*2
            height: sourceComponent && visible? childViewHeight : 0
        }
    }
}
