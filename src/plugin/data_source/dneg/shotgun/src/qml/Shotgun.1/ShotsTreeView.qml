// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQml.Models 2.15

import xStudio 1.1


Item { id: widget
    property alias model: treeModel.baseModel
    property alias rootIndex: treeModel.rootIndex
    property var selectionModel: null
    property var expandedModel: null
    property var itemDoubleClicked: null
    property var scrollTo: null

    property int totalChildCount: 0
    // property var childObj: null
    property bool scrollBarVisibility: true
    property alias listView: treeView

    x: frameWidth + framePadding

    width: parent.width - x
    height: parent.height

    function updateCount() {
        totalChildCount = model.countExpandedChildren(rootIndex, expandedModel.selectedIndexes)
    }

    Connections {
        target: expandedModel ? expandedModel : null
        function onSelectionChanged(selected, deselected) {
            if(expandedModel.selectedIndexes.includes(rootIndex)) {
                updateCount()
            }
        }
    }

    Connections {
        target: selectionModel ? selectionModel : null
        function onSelectionChanged(selected, deselected) {
            jumpToSelected()
        }
    }

    function jumpToSelected() {
        if(selectionModel.selectedIndexes.length && selectionModel.selectedIndexes[0].parent == rootIndex) {
            let item = treeView.itemAtIndex(selectionModel.selectedIndexes[0].row)
            widget.scrollTo(item)
        }
    }

    function createTreeNode(parent, myModel, myRootIndex, mySelectionModel, myExpandedModel, myItemDoubleClicked, myScrollTo) {
        const component = Qt.createComponent("ShotsTreeView.qml");
        const incubator = component.incubateObject(
            parent,
            {
                model: myModel,
                rootIndex: myRootIndex,
                selectionModel: mySelectionModel,
                expandedModel: myExpandedModel,
                x: itemHeight/1.25-frameWidth*2,
                scrollBarVisibility:false,
                itemDoubleClicked: myItemDoubleClicked,
                scrollTo: myScrollTo
            }
        );

        if (incubator.status !== Component.Ready) {
            incubator.onStatusChanged = function(status) {
                if (status === Component.Ready) {
                    incubator.object.updateCount()
                }
            };
        } else {
            incubator.object.updateCount()
        }
    }

    ListView {
        id: treeView
        // property bool isIconVisible: false

        model: treeModel

        anchors.fill: parent
        spacing: 0
        clip: true
        orientation: ListView.Vertical
        interactive: true
        currentIndex: -1
        highlightRangeMode: ListView.ApplyRange
        // snapMode: ListView.SnapToItem
        cacheBuffer: 100000

        ScrollBar.vertical: XsScrollBar {id: scrollBar; x:width; visible: scrollBarVisibility && treeView.height < treeView.contentHeight}

        DelegateModel { id: treeModel
            property var baseModel: null

            function remove(row) {
                baseModel.removeRows(row, 1, rootIndex)
            }

            function move(src_row, dst_row) {
                // console.log("TreeDelegateModel.qml", src_row, "->", dst_row, rootIndex)
                // invert logic if moving down
                if(dst_row > src_row) {
                    baseModel.moveRows(rootIndex, dst_row, 1, rootIndex, src_row)
                } else {
                    baseModel.moveRows(rootIndex, src_row, 1, rootIndex, dst_row)
                }
            }

            function insert(row, data) {
                baseModel.insert(row, rootIndex, data)
            }

            function append(data, root=rootIndex) {
                baseModel.insert(rowCount(root), root, data)
            }

            function rowCount(root=rootIndex) {
                return baseModel.rowCount(root)
            }

            function get(row, role) {
                return baseModel.get(row, rootIndex, role)
            }

            function set(row, value, role) {
                return baseModel.set(row, value, role, rootIndex)
            }

            model: baseModel

            delegate:
            MouseArea { id: treeDelegate
                property bool isExpanded: expandedModel.selectedIndexes.includes(treeModel.modelIndex(index, treeModel.rootIndex, expandedModel))
                property bool isSelected: selectionModel.selectedIndexes.includes(treeModel.modelIndex(index, treeModel.rootIndex, selectionModel))
                property bool isPopulated: false
                property int childCount: treeModel.rowCount(treeModel.modelIndex(index, treeModel.rootIndex))
                // property var isMouseHovered: containsMouse
                readonly property int treeItemHeight: 20

                hoverEnabled: true
                width: treeView.width
                height: isExpanded ? (treeItemHeight + childView.height) : treeItemHeight

                onClicked: {
                    selectionModel.select(treeModel.modelIndex(index, treeModel.rootIndex), ItemSelectionModel.ClearAndSelect)
                }
                onDoubleClicked: {
                    itemDoubleClicked(typeRole, nameRole, idRole)
                }

                onIsExpandedChanged: {
                    if(isExpanded) {
                        if(!isPopulated) {
                            createTreeNode(childView, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex), selectionModel, expandedModel, itemDoubleClicked, scrollTo)
                            isPopulated = true
                        }
                        jumpToSelected()
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle{id: nameDiv
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: treeItemHeight
                        color: isSelected ? Qt.darker(itemColorActive, 2.75) : "transparent"

                        XsButton{id: expandButton

                            text: ""
                            imgSrc: childCount > 0 ? "qrc:/feather_icons/chevron-down.svg" : null
                            width: height
                            height: parent.height - frameWidth*2
                            image.sourceSize.width: height/1.2
                            image.sourceSize.height: height/1.2
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                            opacity: !childCount ? 0 : 1
                            isActive: isExpanded
                            enabled: childCount > 0

                            scale: rotation==0 || rotation==-90? 1 : 0.85
                            rotation: (isExpanded)? 0: -90
                            Behavior on rotation {NumberAnimation{id: rotationAnim; duration: 150 }}

                            onClicked: expandedModel.select(treeModel.modelIndex(index, treeModel.rootIndex), ItemSelectionModel.Toggle)
                            Component.onCompleted: {
                                if(isExpanded) {
                                    createTreeNode(childView, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex), selectionModel, expandedModel, itemDoubleClicked, scrollTo)
                                    isPopulated = true
                                    jumpToSelected()
                                }
                            }
                        }

                        // Image { id: icon
                        //     width: treeView.isIconVisible? height : 0
                        //     height: parent.height - framePadding
                        //     anchors.verticalCenter: parent.verticalCenter
                        //     anchors.left: expandButton.right
                        //     anchors.leftMargin: treeView.isIconVisible? framePadding : 0
                        //     source: childCount > 0 ? "qrc:/feather_icons/archive.svg" : "qrc:/feather_icons/box.svg"
                        //     sourceSize.width: height/1.3
                        //     sourceSize.height: height/1.3
                        //     layer {
                        //         enabled: true
                        //         effect:
                        //         ColorOverlay {
                        //             color: isMouseHovered? textColorNormal : isSelected? itemColorActive : "#404040"
                        //         }
                        //     }
                        // }

                        XsText{
                            text: !isCollapsed? nameRole : nameRole.substr(0,2)+".."
                            width: isCollapsed? parent.width: parent.width - expandButton.width - framePadding*5 //icon.width - framePadding*5
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: expandButton.right//icon.right
                            anchors.leftMargin: framePadding

                            elide: Text.ElideRight
                            font.pixelSize: fontSize*1.2


                            color: isSelected ? (containsMouse ? textColorNormal : textColorActive) : (containsMouse? itemColorActive : textColorNormal)
                            // isSelected? textColorActive : containsMouse? itemColorActive : textColorNormal//itemColorNormal
                            // color: isMouseHovered || presetLoadedRole? textColorActive: textColorNormal


                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            ToolTip.text: nameRole
                            ToolTip.visible: containsMouse && isCollapsed
                            visible: !(treeView.isEditable && treeView.menuActionIndex == index)
                        }

                        // XsButton{id: favButton

                        //     text: ""
                        //     imgSrc: isFavorited? "qrc:/icons/star-filled.svg":"qrc:/feather_icons/star.svg"
                        //     visible: typeRole=="Shot" && (containsMouse || isFavorited)
                        //     width: height
                        //     height: parent.height - frameWidth*2
                        //     image.sourceSize.width: height/1.2
                        //     image.sourceSize.height: height/1.2
                        //     anchors.verticalCenter: parent.verticalCenter
                        //     anchors.right: parent.right
                        //     anchors.rightMargin: itemSpacing*4
                        //     bgColorNormal: "transparent"
                        //     borderWidth: hovered? 1 : 0
                        //     property bool isFavorited: false
                        //     layer {
                        //         textureSize: Qt.size(favButton.width, favButton.height)
                        //         enabled: true
                        //         effect:
                        //         ColorOverlay {
                        //             color: favButton.pressed? "transparent": favButton.isFavorited? itemColorActive : "transparent"
                        //         }
                        //     }

                        //     onClicked: {
                        //         isFavorited= !isFavorited
                        //     }
                        // }
                        // XsButton{id: pinButton

                        //     text: ""
                        //     imgSrc: "qrc:/icons/pin.png"
                        //     visible: typeRole=="Shot" && containsMouse
                        //     width: height
                        //     height: parent.height - frameWidth*2
                        //     image.sourceSize.width: height/1.2
                        //     image.sourceSize.height: height/1.2
                        //     anchors.verticalCenter: parent.verticalCenter
                        //     anchors.right: favButton.left
                        //     anchors.rightMargin: itemSpacing
                        //     bgColorNormal: "transparent"
                        //     borderWidth: hovered? 1 : 0
                        //     // layer {
                        //     //     textureSize: Qt.size(pinButton.width, pinButton.height)
                        //     //     enabled: true
                        //     //     effect:
                        //     //     ColorOverlay {
                        //     //         color: pinButton.hovered? pinButton.textColorPressed : "transparent"
                        //     //     }
                        //     // }
                        //     onClicked: {
                        //     }
                        // }
                    }

                    Item { id: childView
                        // visible: isExpanded
                        Layout.fillWidth: true
                        Layout.minimumWidth: parent.width
                        Layout.fillHeight: true
                        Layout.minimumHeight: isExpanded ? treeItemHeight * children[0].totalChildCount : 0
                        Layout.maximumHeight: isExpanded ? treeItemHeight * children[0].totalChildCount : 0
                    }
                }
            }
        }
    }
}
