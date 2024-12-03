// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

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

    property var delegateModel: null
    property var selectionModel: null

    onClicked: (mouse) => {
        if(mouse.modifiers == Qt.NoModifier) {
            ShotBrowserHelpers.selectItem(selectionModel, delegateModel.index(index, 0))
        } else if(mouse.modifiers == Qt.ShiftModifier){
            ShotBrowserHelpers.shiftSelectItem(selectionModel, delegateModel.index(index, 0))
        } else if(mouse.modifiers == Qt.ControlModifier) {
            ShotBrowserHelpers.ctrlSelectItem(selectionModel, delegateModel.index(index, 0))
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
            onClicked: (mouse)=>{
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

                    onClicked: {
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
                    visible: showStatus
                    text: statusRole  ? statusRole : ""
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: XsStyleSheet.fontSize*1.2
                    elide: Text.ElideRight
                    rightPadding: 8
                }
            }
        }
    }
}

