// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQuick.Dialogs 1.3
import QtGraphicalEffects 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import ShotBrowser 1.0

Item{
    id: panel
    anchors.fill: parent

    property bool isPanelEnabled: true
    property var dataModel: results

    property real panelPadding: XsStyleSheet.panelPadding
    property color panelColor: XsStyleSheet.panelBgColor

    property var activeScopeIndex: ShotBrowserEngine.presetsModel.index(-1,-1)

    // Track the uuid of the media that is currently visible in the Viewport
    property var onScreenMediaUuid: currentPlayhead.mediaUuid
    property var onScreenLogicalFrame: currentPlayhead.logicalFrame

    property int queryCounter: 0
    property int queryRunning: 0
    readonly property string panelType: "ShotHistory"

    property alias nameFilter: results.filterName
    property string sentTo: ""

    property bool isPopout: typeof panels_layout_model_index == "undefined"

    // used ?
    property real btnHeight: XsStyleSheet.widgetStdHeight + 4

    property bool isPaused: false

    onOnScreenMediaUuidChanged: {if(visible) updateTimer.start()}

    onSentToChanged: {
        runQuery()
    }

    onOnScreenLogicalFrameChanged: {
        if(updateTimer.running) {
            updateTimer.restart()
            if(isPanelEnabled && !isPaused) {
                isPaused = true
            }
        }
    }

    Timer {
        id: updateTimer
        interval: 500
        running: false
        repeat: false
        onTriggered: {
            isPaused = false
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            // if(onScreenMediaUuid == "{00000000-0000-0000-0000-000000000000}") {
            //     results.setResultData([])
            // }
        }
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            setIndexFromPreference()
        }
    }

    function setIndexFromPreference() {
        if(ShotBrowserEngine.ready  && !activeScopeIndex.valid && prefs.scope) {
            // from panel.
            if(prefs.scope != undefined) {
                let i = getScopeIndex(prefs.scope)
                if(i.valid && activeScopeIndex != i)
                    activeScopeIndex = i
            }
        }
        runQuery()
    }
    function getScopeIndex(scope_name) {
        let m = ShotBrowserEngine.presetsModel
        let p = m.searchRecursive("c5ce1db6-dac0-4481-a42b-202e637ac819", "idRole", ShotBrowserEngine.presetsModel.index(-1, -1), 0, 1)
        return m.searchRecursive(scope_name, "nameRole", p, 0, 0)
    }

    onActiveScopeIndexChanged: {
        if(activeScopeIndex && activeScopeIndex.valid) {
            let m = activeScopeIndex.model
            let i = m.get(activeScopeIndex, "nameRole")
            prefs.scope = i
        }
    }

    Connections {
        target: ShotBrowserEngine
        function onLiveLinkMetadataChanged() {
            if(!isPaused && isPanelEnabled && panel.visible) {
                runQuery()
            }
        }
    }

    onIsPanelEnabledChanged: {
        if(isPanelEnabled) {
            if(!ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid))
                runQuery()
        }
    }

    Component.onCompleted: {
        if(visible) {
            ShotBrowserEngine.connected = true
            if(!ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid))
                runQuery()
        }
        setIndexFromPreference()
    }

    onVisibleChanged: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setIndexFromPreference()
            if(!ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid))
                runQuery()
        }
    }

    Item {
        // Hold properties that we want to persist between sessions.
        id: prefs
        property string scope: ""

        XsStoredPanelProperties {
            propertyNames: ["scope"]
            onPropertiesInitialised: {
                prefs.initialised = true
                setIndexFromPreference()
            }
        }
        property bool initialised: false

    }

    function runQuery() {
        if(isPanelEnabled && activeScopeIndex.valid) {
            // make sure the results appear in sync.
            let custom = []
            if(sentTo != "" && sentTo != "Ignore")
                custom.push({
                    "enabled": true,
                    "type": "term",
                    "term": "Sent To",
                    "value": sentTo
                })

            if(onScreenMediaUuid == "{00000000-0000-0000-0000-000000000000}") {
                resultsBaseModel.setResultData([])
            } else {
                queryCounter += 1
                queryRunning += 1
                let i = queryCounter
                Future.promise(
                    ShotBrowserEngine.executeQuery(
                        [ShotBrowserEngine.presetsModel.get(activeScopeIndex, "jsonPathRole")],
                        {},
                        custom
                    )
                ).then(
                    function(json_string) {
                        if(queryCounter == i) {
                            resultsSelectionModel.clear()
                            resultsBaseModel.setResultData([json_string])
                        }
                        queryRunning -= 1
                    },
                    function() {
                        resultsSelectionModel.clear()
                        resultsBaseModel.setResultData([])
                        queryRunning -= 1
                    }
                )
            }
        }
    }

    function activateScope(clickedIndex){
        if(clickedIndex.valid) {
            activeScopeIndex = clickedIndex
            runQuery()
        }
    }

    XsGradientRectangle{ id: backgroundDiv
        anchors.fill: parent
    }

    ShotHistoryResultPopup {
        id: resultPopup
        menu_model_name: "shot_history_popup"+resultPopup
        popupSelectionModel: resultsSelectionModel
    }

    ShotBrowserResultModel {
        id: resultsBaseModel
    }

    ShotBrowserResultFilterModel {
        id: results
        sourceModel: resultsBaseModel
    }


    ItemSelectionModel {
        id: resultsSelectionModel
        model: results
    }

    ColumnLayout{
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        ShotHistoryTitleDiv{id: titleDiv
            titleButtonHeight: (XsStyleSheet.widgetStdHeight + 4)
            Layout.fillWidth: true
            Layout.minimumHeight: titleButtonHeight*2
            Layout.maximumHeight: titleButtonHeight*2
        }

        Rectangle{
            color: panelColor
            Layout.fillWidth: true
            Layout.fillHeight: true

            ShotHistoryListDiv{
                anchors.fill: parent
                anchors.margins: 4
            }
        }

        ShotHistoryActionDiv{id: buttonsDiv
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            parentWidth: parent.width - 4
        }
    }
}