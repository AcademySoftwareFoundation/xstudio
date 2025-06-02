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
    property bool isSelected: selectionModel.isSelected(delegateModel.index(index, 0))
    property bool showUnit: false
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
        }
    }

    Connections {
        target: selectionModel
        function onSelectionChanged(selected, deselected) {
            isSelected = selectionModel.isSelected(delegateModel.index(index, 0))
        }
    }

    Rectangle {
        id: treeNode
        color: "transparent"
        anchors.fill: parent
        anchors.leftMargin: (delegateModel.depthAtRow(index)+1) * height/1.2

        MouseArea{ id: nodeDivMArea
            anchors.fill: nodeDiv
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
                    visible: showUnit

                    text: unitRole  ? unitRole : ""
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
                    visible: showType

                    text: subtypeRole ? subtypeRole : ""
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
                    opacity: parentHiddenRole ? 0.4 : 0.7

                    // showHoverOnActive: favouriteRole && !thisItem.hovered
                    isColoured: hiddenRole// && thisItem.hovered
                    imgSrc: hiddenRole ? "qrc:///icons/visibility_off.svg" : "qrc:///icons/visibility.svg"
                    scale: 0.95
                    onClicked: {
                        hiddenRole = !hiddenRole
                        updateHidden()
                    }
                }
            }
        }
    }
}

