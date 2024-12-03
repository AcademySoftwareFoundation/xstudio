// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0

ColumnLayout{
    id: toolDiv
    spacing: panelPadding

    RowLayout{
        spacing: buttonSpacing*2
        Layout.fillWidth: true
        Layout.minimumHeight: btnHeight
        Layout.maximumHeight: btnHeight

        Repeater {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height
            model: DelegateModel {
                id: delegate_model
                property var notifyModel: currentCategory ==  "Tree" ? treeButtonModel : (currentCategory ==  "Recent" ?  recentButtonModel : menuButtonModel)
                model: notifyModel
                delegate: XsPrimaryButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: nameRole
                    toolTip:  getPresetIndex().valid ? getPresetIndex().model.get(getPresetIndex().parent.parent, "nameRole") + " / " + nameRole : ""

                    function getPresetIndex() {
                        let tindex = delegate_model.notifyModel.mapToSource(delegate_model.modelIndex(index))
                        if(tindex.valid)
                            return tindex.model.mapToModel(tindex)
                        return tindex
                    }
                    property bool isRunning: queryRunning && isActive

                    isActive: currentPresetIndex == getPresetIndex()
                    onClicked: {
                        activatePreset(getPresetIndex())
                        presetsSelectionModel.select(getPresetIndex(), ItemSelectionModel.ClearAndSelect)
                    }

                    XsBusyIndicator{ id: busyIndicator
                        x: 4
                        width: height
                        height: parent.height
                        running: visible
                        visible: isRunning
                        scale: 0.5
                    }
                }
            }
        }
    }

    Rectangle{
        Layout.fillWidth: true
        Layout.preferredHeight: 2
        color: panelColor
    }

    RowLayout{
        spacing: buttonSpacing
        Layout.fillWidth: true
        Layout.minimumHeight: btnHeight
        Layout.maximumHeight: btnHeight

        XsSearchButton{ id: filterBtn
            Layout.preferredHeight: parent.height
            Layout.preferredWidth: isExpanded ? parent.width : btnWidth
            Layout.minimumWidth: btnWidth

            Layout.maximumWidth: isExpanded ? parent.width : btnWidth

            Layout.fillWidth: true
            isExpanded: true
            hint: "Filter"
            onTextChanged: nameFilter = text
            onEditingCompleted: focus = false

            Connections {
                target: panel
                function onNameFilterChanged() {
                    filterBtn.text = nameFilter
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        MouseArea {
            Layout.preferredWidth: btnWidth*2.5
            Layout.preferredHeight: parent.height

            hoverEnabled: true

            XsSBCountDisplay{
                anchors.fill: parent
                filteredCount: resultsFilteredModel.count
                totalCount: resultsBaseModel.count == undefined ? "-" : resultsBaseModel.truncated ? resultsBaseModel.count + "+" : resultsBaseModel.count
            }

            XsToolTip {
                timeout: 0
                visible: parent.containsMouse
                text: "Execution time " + resultsBaseModel.executionMilliseconds + " ms."
            }
        }

        XsComboBoxEditable{ id: filterStep
            Layout.minimumWidth: btnWidth*3
            Layout.preferredWidth: btnWidth*3
            Layout.preferredHeight: parent.height
            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Pipeline Step") : []
            textRole: "nameRole"
            currentIndex: -1
            displayText: currentIndex ==-1 ? "Pipeline Step" : currentText

            onModelChanged: currentIndex = -1

            onCurrentIndexChanged: {
                if(currentIndex == -1)
                    pipeStep = ""
            }
            onAccepted: {
                pipeStep = model.get(model.index(currentIndex, 0), "nameRole")
                focus = false
            }

            onActivated: pipeStep = model.get(model.index(currentIndex,0), "nameRole")

            Connections {
                target: panel
                function onPipeStepChanged() {
                    filterStep.currentIndex = filterStep.find(pipeStep)
                }
            }
        }
        XsComboBoxEditable{ id: filterOnDisk
            Layout.minimumWidth: btnWidth*2
            Layout.preferredWidth: btnWidth*2
            Layout.preferredHeight: parent.height
            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Site") : []
            currentIndex: -1
            textRole: "nameRole"
            displayText: currentIndex==-1? "On Disk" : currentText

            onModelChanged: currentIndex = -1

            onCurrentIndexChanged: {
                if(currentIndex==-1)
                    onDisk = ""
            }

            onAccepted: {
                onDisk = model.get(model.index(currentIndex,0), "nameRole")
                focus = false
            }

            onActivated: onDisk = model.get(model.index(currentIndex,0), "nameRole")

            Connections {
                target: panel
                function onOnDiskChanged() {
                    filterOnDisk.currentIndex = filterOnDisk.find(onDisk)
                }
            }

        }


        XsButtonWithImageAndText{ id: groupBtn
            Layout.preferredWidth: btnWidth*2.2
            Layout.preferredHeight: parent.height
            iconSrc: "qrc:///shotbrowser_icons/account_tree.svg"
            iconText: "Group"
            textDiv.visible: true
            isActive: resultsBaseModel.isGrouped
            visible: resultsBaseModel.canBeGrouped
            onClicked: resultsBaseModel.isGrouped = !resultsBaseModel.isGrouped
        }

        XsSortButton{ id: sortViaNaturalOrderBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: parent.height
            text: "ShotGrid Order"
            isActive: sortByNaturalOrder
            sortIconText: "SG"
            isDescendingOrder: sortByNaturalOrder && !sortInAscending

            onClicked: {
                if(sortByNaturalOrder && sortInAscending) sortInAscending = false
                else sortInAscending = true
                sortByNaturalOrder = true
            }
        }

        XsSortButton{ id: sortViaDateBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: parent.height
            text: "Creation Date"
            isActive: sortByCreationDate
            sortIconSrc: "qrc:///shotbrowser_icons/calendar_month.svg"
            isDescendingOrder: sortByCreationDate && !sortInAscending

            onClicked: {
                if(sortByCreationDate && sortInAscending) sortInAscending = false
                else sortInAscending = true
                sortByCreationDate = true
            }
        }

        XsSortButton{ id: sortViaShotBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: parent.height
            text: "Shot Name"
            isActive: sortByShotName
            sortIconText: "AZ"
            isDescendingOrder: sortByShotName && !sortInAscending

            onClicked: {
                if(sortByShotName && sortInAscending) sortInAscending = false
                else sortInAscending = true
                sortByShotName = true
            }
        }
    }
}
