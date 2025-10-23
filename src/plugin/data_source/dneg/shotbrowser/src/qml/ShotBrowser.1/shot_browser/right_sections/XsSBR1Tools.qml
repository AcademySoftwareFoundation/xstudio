// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

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
            Layout.preferredHeight: btnHeight
            model: DelegateModel {
                id: delegate_model
                property var notifyModel: currentCategory ==  "Tree" ? treeButtonModel :  menuButtonModel
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

        XsSearchBar{ id: filterBtn
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 50

            placeholderText: "Filter..."
            onTextChanged: {
                nameFilter = text
                defocus.restart()
            }

            onHoveredChanged: {
                if(focus && !hovered)
                    defocus.restart()
            }

            onFocusChanged: {
                if(focus)
                    defocus.restart()
            }

            Timer {
                id: defocus
                interval: 5000
                running: false
                repeat: false
                onTriggered: {
                    if(filterBtn.hovered)
                        start()
                    else
                        filterBtn.focus = false
                }
            }

            Connections {
                target: panel
                function onNameFilterChanged() {
                    filterBtn.text = nameFilter
                }
            }
        }

        MouseArea {
            Layout.preferredWidth: btnWidth*2.5
            Layout.fillHeight: true

            hoverEnabled: true

            XsSBCountDisplay{
                anchors.fill: parent
                filteredCount: resultsFilteredModel.count
                totalCount: resultsBaseModel.count == undefined ? "-" : resultsBaseModel.truncated ? resultsBaseModel.count + "+" : resultsBaseModel.count
            }

            XsToolTip {
                delay: 0
                visible: parent.containsMouse
                text: "Execution time " + resultsBaseModel.executionMilliseconds + " ms."
            }
        }

        XsComboBoxEditable{ id: filterStep
            Layout.minimumWidth: btnWidth/2
            Layout.preferredWidth: btnWidth*3
            Layout.maximumWidth: btnWidth*3
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Pipeline Step") : []
            textRole: "nameRole"
            currentIndex: -1
            placeholderText: "Pipeline Step"

            onModelChanged: currentIndex = -1

            onCurrentIndexChanged: {
                if(currentIndex == -1)
                    pipeStep = ""
            }
            onAccepted: {
                if(currentIndex != -1)
                    pipeStep = model.get(model.index(currentIndex, 0), "nameRole")
                focus = false
            }

            onActivated: {
                if(currentIndex != -1)
                    pipeStep = model.get(model.index(currentIndex,0), "nameRole")
            }

            Connections {
                target: panel
                function onPipeStepChanged() {
                    filterStep.currentIndex = filterStep.find(pipeStep)
                }
            }
            Connections {
                target: panel
                function onCurrentPresetIndexChanged() {
                    filterStep.currentIndex = -1
                }
            }
        }
        XsComboBoxEditable{ id: filterOnDisk
            Layout.fillHeight: true
            Layout.fillWidth: true

            Layout.minimumWidth: btnWidth/2
            Layout.preferredWidth: btnWidth*2.2
            Layout.maximumWidth: btnWidth*2.2

            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Site") : []
            currentIndex: -1
            textRole: "nameRole"
            placeholderText: "On Disk"

            onModelChanged: currentIndex = -1

            onCurrentIndexChanged: {
                if(currentIndex==-1)
                    onDisk = ""
            }

            onAccepted: {
                if(currentIndex != -1)
                    onDisk = model.get(model.index(currentIndex,0), "nameRole")
                focus = false
            }

            onActivated: {
                if(currentIndex != -1)
                    onDisk = model.get(model.index(currentIndex,0), "nameRole")
            }

            Connections {
                target: panel
                function onOnDiskChanged() {
                    filterOnDisk.currentIndex = filterOnDisk.find(onDisk)
                }
            }
        }

        XsPrimaryButton{ id: groupBtn
            Layout.leftMargin: 2
            Layout.rightMargin: 2
            Layout.preferredWidth: btnWidth
            Layout.fillHeight: true

            imgSrc: "qrc:///shotbrowser_icons/account_tree.svg"
            toolTip: "Group By Version"
            isActive: resultsBaseModel.isGrouped
            visible: resultsBaseModel.canBeGrouped
            onClicked: resultsBaseModel.isGrouped = !resultsBaseModel.isGrouped
        }

        XsSortButton{ id: sortViaNaturalOrderBtn
            Layout.preferredWidth: btnWidth
            Layout.fillHeight: true
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
            Layout.fillHeight: true
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
            Layout.fillHeight: true
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
