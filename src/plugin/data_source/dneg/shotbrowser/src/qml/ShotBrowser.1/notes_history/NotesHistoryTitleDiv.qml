// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

RowLayout {

    readonly property real titleButtonSpacing: 1
    property real titleButtonHeight: XsStyleSheet.widgetStdHeight+4

    ShotBrowserPresetFilterModel {
        id: scopeFilterModel
        showHidden: false
        onlyShowFavourite: true
        filterUserData: "scope"

        sourceModel: ShotBrowserEngine.presetsModel
    }

    ShotBrowserPresetFilterModel {
        id: typeFilterModel
        showHidden: false
        onlyShowFavourite: true
        filterUserData: "type"

        sourceModel: ShotBrowserEngine.presetsModel
    }

    // make sure index is updated
    XsModelProperty {
        index: scopeGroupModel.rootIndex
        onIndexChanged: {
            if(!index.valid) {
                // clear list views
                typeList.model = []
                scopeList.model = []
            }
            populateModels()
        }
    }


    Timer {
        id: populateTimer
        interval: 100
        running: false
        repeat: false
        onTriggered: {
            let sourceInd = ShotBrowserEngine.presetsModel.searchRecursive(
                "aac8207e-129d-4988-9e05-b59f75ae2f75", "idRole", ShotBrowserEngine.presetsModel.index(-1, -1), 0, 1
            )
            let sind = scopeFilterModel.mapFromSource(sourceInd)
            let tind = typeFilterModel.mapFromSource(sourceInd)

            if(sind.valid && tind.valid) {
                scopeGroupModel.rootIndex = sind
                typeGroupModel.rootIndex = tind

                scopeList.model = scopeGroupModel
                typeList.model = typeGroupModel

            } else {
                scopeList.model = []
                typeList.model = []
                populateTimer.start()
            }
        }
    }


    DelegateModel {
        id: scopeGroupModel

        property var srcModel: scopeFilterModel
        onSrcModelChanged: model = srcModel

        delegate: XsPrimaryButton {
            width: ListView.view.width / ListView.view.count
            height: titleButtonHeight
            text: nameRole
            isActive: activeScopeIndex == scopeGroupModel.srcModel.mapToSource(scopeGroupModel.modelIndex(index))

            onClicked: {
                activateScope(scopeGroupModel.srcModel.mapToSource(scopeGroupModel.modelIndex(index)))
            }
        }
    }

    DelegateModel {
        id: typeGroupModel

        property var srcModel: typeFilterModel
        onSrcModelChanged: model = srcModel

        delegate: XsPrimaryButton{
            width: ListView.view.width / ListView.view.count
            height: titleButtonHeight
            text: nameRole
            isActive: activeTypeIndex == typeGroupModel.srcModel.mapToSource(typeGroupModel.modelIndex(index))

            onClicked: {
                activateType(typeGroupModel.srcModel.mapToSource(typeGroupModel.modelIndex(index)))
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

    XsPrimaryButton {
        Layout.preferredWidth: 40
        Layout.fillHeight: true

        imgSrc: isPanelEnabled && !isPaused ? "qrc:///shotbrowser_icons/lock_open.svg" : "qrc:///shotbrowser_icons/lock.svg"
        // text: isPanelEnabled? "ON" : "OFF"
        isActive: !isPanelEnabled || isPaused
        onClicked: {
            isPanelEnabled = !isPanelEnabled
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        spacing: titleButtonSpacing

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            // width: parent.width
            // height: titleButtonHeight
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
                model: []
            }
        }

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            // width: parent.width
            // height: titleButtonHeight
            spacing: 0

            XsText{
                Layout.preferredWidth: scopeTxt.width
                Layout.fillHeight: true
                text: "Type: "
            }

            XsListView{ id: typeList
                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: titleButtonSpacing
                orientation: ListView.Horizontal
                enabled: isPanelEnabled && !isPaused
                model: []
            }
        }
    }
}