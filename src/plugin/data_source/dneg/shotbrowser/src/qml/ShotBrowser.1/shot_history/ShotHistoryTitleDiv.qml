// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

RowLayout {id: titleDiv

    property int titleButtonCount: 4
    property real titleButtonSpacing: 1
    property real titleButtonHeight: XsStyleSheet.widgetStdHeight+4

    ShotBrowserPresetFilterModel {
        id: filterModel
        showHidden: false
        onlyShowFavourite: true
        sourceModel: ShotBrowserEngine.presetsModel
    }

    DelegateModel {
        id: groupModel
        property var srcModel: filterModel
        onSrcModelChanged: model = srcModel

        delegate: XsPrimaryButton{
            width: ListView.view.width / ListView.view.count
            height: titleButtonHeight
            text: nameRole
            isActive: activeScopeIndex == groupModel.srcModel.mapToSource(groupModel.modelIndex(index))

            onClicked: {
                activateScope(groupModel.srcModel.mapToSource(groupModel.modelIndex(index)))
            }
        }
    }

    // make sure index is updated
    XsModelProperty {
        index: groupModel.rootIndex
        onIndexChanged: {
            if(!index.valid)
                scopeList.model = []
            populateModels()
        }
    }

    Timer {
        id: populateTimer
        interval: 100
        running: false
        repeat: false
        onTriggered: {
            let ind = filterModel.mapFromSource(ShotBrowserEngine.presetsModel.searchRecursive(
                "c5ce1db6-dac0-4481-a42b-202e637ac819", "idRole", ShotBrowserEngine.presetsModel.index(-1, -1), 0, 1
            ))
            if(ind.valid) {
                groupModel.rootIndex = ind
                scopeList.model = groupModel
            } else {
                scopeList.model = []
                populateTimer.start()
            }
        }
    }

    function populateModels() {
        populateTimer.start()
    }

    Component.onCompleted: {
        if(ShotBrowserEngine.ready)
            populateModels()
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            if(ShotBrowserEngine.ready)
                 populateModels()
        }
    }

    XsPrimaryButton{ id: updateScopeBtn
        Layout.preferredWidth: 40
        Layout.fillHeight: true

        imgSrc: isPanelEnabled && !isPaused ? "qrc:///shotbrowser_icons/lock_open.svg" : "qrc:///shotbrowser_icons/lock.svg"
        // text: isPanelEnabled? "ON" : "OFF"
        isActive: !isPanelEnabled || isPaused
        onClicked: isPanelEnabled = !isPanelEnabled
    }

    ColumnLayout{ id: col
        Layout.fillWidth: true
        Layout.fillHeight: true

        spacing: titleButtonSpacing

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            spacing: 0


            XsText{ id: scopeTxt
                Layout.preferredWidth: (textWidth + panelPadding*3)
                Layout.fillHeight: true
                text: "Scope: "
            }

            XsListView{ id: scopeList
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: titleButtonSpacing

                orientation: ListView.Horizontal
                enabled: isPanelEnabled && !isPaused
            }
        }
        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            spacing: 0

            XsSearchButton{ id: filterBtn
                Layout.fillWidth: true
                Layout.fillHeight: true
                isExpanded: true
                hint: "Filter"
                buttonWidth: scopeTxt.width
                enabled: isPanelEnabled && !isPaused

                onTextChanged: nameFilter = text
                onEditingCompleted: forceActiveFocus(panel)

                Connections {
                    target: panel
                    function onNameFilterChanged() {
                        filterBtn.text = nameFilter
                    }
                }
            }

            XsComboBoxEditable{ id: filterSentTo
                Layout.fillHeight: true
                Layout.minimumWidth: titleButtonHeight * 3
                Layout.preferredWidth: titleButtonHeight * 3

                enabled: isPanelEnabled && !isPaused

                model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Sent To") : []
                currentIndex: -1
                textRole: "nameRole"
                displayText: currentIndex==-1? "Sent To" : currentText

                onModelChanged: currentIndex = -1

                onCurrentIndexChanged: {
                    if(currentIndex==-1)
                        sentTo = ""
                }

                onAccepted: {
                    sentTo = model.get(model.index(currentIndex,0), "nameRole")
                }

                onActivated: sentTo = model.get(model.index(currentIndex,0), "nameRole")

                Connections {
                    target: panel
                    function onSentToChanged() {
                        filterSentTo.currentIndex = filterSentTo.find(sentTo)
                    }
                }
            }
        }
    }
}