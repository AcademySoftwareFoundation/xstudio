// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQml.Models
import Qt.labs.qmlmodels

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

MouseArea {
    hoverEnabled: true

    property bool isHovered: nodeDivMArea.containsMouse
    property bool isExpanded: isParent && delegateModel.isExpanded(index)
    property bool isSelected: selectionModel.isSelected(delegateModel.index(index, 0))

    property bool isParent: delegateModel.model.rowCount(delegateModel.mapRowToModel(index))

    property bool showStatus: false
    property bool showType: false
    property bool showVisibility: false

    property var delegateModel: null
    property var selectionModel: null

    onClicked: (mouse) => {
        if(mouse.modifiers == Qt.NoModifier) {
            ShotBrowserHelpers.selectItem(selectionModel, delegateModel.index(index, 0))
        } else if(mouse.modifiers == Qt.ShiftModifier){
            ShotBrowserHelpers.shiftSelectItem(selectionModel, delegateModel.index(index, 0))
        } else if(mouse.modifiers == Qt.ControlModifier) {
            ShotBrowserHelpers.ctrlSelectItem(selectionModel, delegateModel.index(index, 0))
        } else if(mouse.modifiers == Qt.AltModifier) {
            isExpanded = isParent
            ShotBrowserHelpers.altSelectItem(selectionModel, delegateModel.index(index, 0))
        }
    }

    Connections {
        target: delegateModel.model
        function onFilterChanged() {
            isParent = delegateModel.model.rowCount(delegateModel.mapRowToModel(index))
        }
    }

    Connections {
        target: selectionModel
        function onSelectionChanged(selected, deselected) {
            isSelected = selectionModel.isSelected(delegateModel.index(index, 0))
        }
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: delegateModel.depthAtRow(index) * height/1.2

        MouseArea{ id: nodeDivMArea
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true
            onClicked: (mouse)=> {
                mouse.accepted = false
            }
        }

        Rectangle{ id: nodeDiv
            color: isSelected ? Qt.darker(palette.highlight, 2) : "transparent"
            border.width: 1
            border.color: isHovered? palette.highlight : "transparent"
            anchors.fill: parent

            RowLayout{
                spacing: buttonSpacing
                anchors.fill: parent

                XsSecondaryButton{ id: expandButton

                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    Layout.preferredWidth: height/1.2
                    Layout.preferredHeight: parent.height

                    imgSrc: "qrc:/icons/chevron_right.svg"
                    isActive: isExpanded
                    enabled: isParent
                    opacity: enabled? 1 : 0.5
                    imageDiv.rotation: (isExpanded)? 90:0


                    // TapHandler {
                    //     acceptedModifiers: Qt.ControlModifier
                    //     onTapped: {
                    //         if(isExpanded) {
                    //             delegateModel.collapseRecursively(index)
                    //             isExpanded =  delegateModel.isExpanded(index)
                    //         }
                    //         else {
                    //             delegateModel.expandRecursively(index,-1)
                    //             isExpanded =  delegateModel.isExpanded(index)
                    //         }
                    //     }
                    // }

                    TapHandler {
                        acceptedModifiers: Qt.ShiftModifier
                        onTapped: {
                            if(isExpanded) {
                                delegateModel.collapseRecursively(index)
                                isExpanded =  delegateModel.isExpanded(index)
                            }
                            else {
                                delegateModel.expandRecursively(index,-1)
                                isExpanded =  delegateModel.isExpanded(index)
                            }
                        }
                    }

                    TapHandler {
                        acceptedModifiers: Qt.NoModifier
                        onTapped: {
                            if(isExpanded) {
                                delegateModel.collapseRow(index)
                                isExpanded =  delegateModel.isExpanded(index)
                            }
                            else {
                                delegateModel.expandRow(index)
                                isExpanded =  delegateModel.isExpanded(index)
                            }
                        }
                    }
                }

                XsText{
                    Layout.fillWidth: true
                    Layout.preferredHeight: parent.height
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    opacity: ["na", "del", "omt", "omtnto", "omtnwd"].includes(statusRole) ? 0.5 : 1.0

                    color: "hld" == statusRole ? "red" : palette.text

                    text: nameRole
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: XsStyleSheet.fontSize*1.2
                    elide: Text.ElideRight
                    leftPadding: 2
                }

                XsText{
                    Layout.preferredHeight: parent.height
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    opacity: 0.5
                    visible: showType

                    text: subtypeRole  ? subtypeRole : ""
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: XsStyleSheet.fontSize*1.2
                    elide: Text.ElideRight
                    rightPadding: 8
                }

                XsText{
                    Layout.preferredHeight: parent.height
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    opacity: 0.5
                    visible: showStatus
                    text: statusRole  ? statusRole : ""
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: XsStyleSheet.fontSize*1.2
                    elide: Text.ElideRight
                    rightPadding: 8
                }

                XsSecondaryButton{ id: favBtn
                    Layout.topMargin: 1
                    Layout.bottomMargin: 1
                    Layout.preferredHeight: parent.height
                    Layout.preferredWidth: parent.height

                    visible: showVisibility

                    isColoured: hiddenRole
                    imgSrc: hiddenRole ? "qrc:///icons/visibility_off.svg" : "qrc:///icons/visibility.svg"
                    scale: 0.95

                    opacity: parentHiddenRole ? 0.4 : 0.7

                    TapHandler {
                        acceptedModifiers: Qt.ControlModifier
                        onTapped: {
                            if(isParent) {
                                let myindex = delegateModel.mapRowToModel(index)
                                for(let i = 0; i < delegateModel.model.rowCount(myindex); i++) {
                                    let cindex = myindex.model.index(i, 0, myindex)
                                    myindex.model.set(cindex, !myindex.model.get(cindex,"hiddenRole"), "hiddenRole")
                                }

                                updateHidden()
                            }
                        }
                    }

                    TapHandler {
                        acceptedModifiers: Qt.ShiftModifier
                        onTapped: {
                            if(isParent) {
                                let myindex = delegateModel.mapRowToModel(index)
                                for(let i = 0; i < delegateModel.model.rowCount(myindex); i++) {
                                    let cindex = myindex.model.index(i, 0, myindex)
                                    myindex.model.set(cindex, hiddenRole, "hiddenRole")
                                }

                                updateHidden()
                            }
                        }
                    }

                    TapHandler {
                        acceptedModifiers: Qt.NoModifier
                        onTapped: {
                            hiddenRole = !hiddenRole
                            updateHidden()
                        }
                    }
                }
            }
        }
    }
}

