// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQml.Models 2.14
import xStudio 1.1


Item {
    property alias model: treeModel.model
    property alias rootIndex: treeModel.rootIndex
    property var selection: null

    function createTreeNode(parent, myModel, myRootIndex, myselection) {
        if (Qt.createComponent("TreeNode.qml").createObject(parent, {model: myModel, rootIndex: myRootIndex, selection:myselection}) == null) {
            console.log("Error creating object");
        }
    }
    anchors.fill: parent

    ListView {
        id: control
        model: treeModel
        anchors.fill: parent

        TreeDelegateModel {
            id: treeModel

            delegate:
            Rectangle {
                color: "transparent"
                width: control.width
                height: 20 * (hasModelChildren ? 5 : 1)
                property bool expanded: false
                property bool populated: false
                property bool isSelected: selection.selectedIndexes.includes(treeModel.modelIndex(index, treeModel.rootIndex))


                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillHeight: false
                        Layout.fillWidth: false
                        Layout.preferredHeight: 20

                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: ""
                            imgSrc: "qrc:/feather_icons/chevron-down.svg"
                            visible: hasModelChildren
                            image.sourceSize.width: height/1.3
                            image.sourceSize.height: height/1.3
                            borderColorNormal: Qt.lighter(palette.base, 0.3)
                            // isActive: isExpanded

                            scale: rotation==0 || rotation==-90?1:0.85
                            rotation: expanded ? 0: -90
                            Behavior on rotation {NumberAnimation{id: rotationAnim; duration: 150 }}

                            onClicked: {
                                if(!expanded) {
                                    expanded = true
                                    if(!populated) {
                                        createTreeNode(children, treeModel.model, treeModel.modelIndex(index, treeModel.rootIndex), selection)
                                        populated = true
                                    }
                                } else {
                                    expanded = false
                                }
                            }
                        }

                        Label {
                            text: display
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                        }
                        Label {
                            text: datatypeRole ? datatypeRole : ""
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                        }
                        Label {
                            text: datatypeRole != "group" && contextRole ? contextRole : ""
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: isSelected ? "S" : "s"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                selection.select(treeModel.modelIndex(index, treeModel.rootIndex), ItemSelectionModel.Toggle)
                            }
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: "R"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                console.log(treeModel.model.removeRows(index, 1, treeModel.rootIndex))
                            }
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: "I"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                console.log(treeModel.model.insertRows(index, 1, treeModel.rootIndex))
                            }
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: "U"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                console.log(treeModel.model.moveRows(treeModel.rootIndex, index, 1, treeModel.rootIndex, index-1))
                            }
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: "D"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                console.log(treeModel.model.moveRows(treeModel.rootIndex, index+1, 1, treeModel.rootIndex, index))
                            }
                        }
                        XsButton {
                            Layout.fillHeight: false
                            Layout.fillWidth: false
                            Layout.preferredWidth: parent.height
                            Layout.preferredHeight: parent.height

                            text: "P"
                            borderColorNormal: Qt.lighter(palette.base, 0.3)

                            onClicked: {
                                console.log(treeModel.model.moveRows(treeModel.rootIndex, index, 1, treeModel.parentModelIndex(), 0))
                            }
                        }
                    }

                    Item {
                        id: children

                        visible: expanded
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.minimumHeight: 80
                    }
                }
            }
        }
    }
}


// hasModelChildren role property to determine whether a node has child nodes.
// DelegateModel::rootIndex allows the root node to be specified
// DelegateModel::modelIndex() returns a QModelIndex which can be assigned to DelegateModel::rootIndex
// DelegateModel::parentModelIndex() returns a QModelIndex which can be assigned to DelegateModel::rootIndex
