// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

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

    property var activeScopeIndex: helpers.qModelIndex()

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

    XsPreference {
        id: addAfterSelection
        path: "/plugin/data_source/shotbrowser/add_after_selection"
    }

    XsPreference {
        id: pauseOnPlaying
        path: "/plugin/data_source/shotbrowser/pause_update_on_playing"
    }

    onOnScreenLogicalFrameChanged: {
        if(visible) {
            if((currentPlayhead.playing || currentPlayhead.scrubbingFrames) && !pauseOnPlaying.value) {
                // no op
            } else if(updateTimer.running) {
                updateTimer.restart()
                if(isPanelEnabled && !isPaused) {
                    isPaused = true
                }
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
        }
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            setIndexFromPreference()
            runQuery()
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
    }

    function getScopeIndex(scope_name) {
        for (const ind of ShotBrowserEngine.presetsModel.shotHistoryScope) {
            if(ShotBrowserEngine.presetsModel.get(ind, "nameRole") == scope_name){
                return ind
            }
        }
        return helpers.qModelIndex()
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
            runQuery()
        }
    }

    onIsPanelEnabledChanged: {
        if(isPanelEnabled) {
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            runQuery()
        }
    }

    Component.onCompleted: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setIndexFromPreference()
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            isPaused = false
            runQuery()
        } else
            setIndexFromPreference()
    }

    onVisibleChanged: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setIndexFromPreference()
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            isPaused = false
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
                runQuery()
            }
        }
        property bool initialised: false

    }

    function runQuery() {
        if(!isPaused && panel.visible && ShotBrowserEngine.ready && isPanelEnabled && activeScopeIndex.valid) {
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
                resultsBaseModel.setResultDataJSON([])
            } else {
                queryCounter += 1
                queryRunning += 1
                let i = queryCounter


                let customContext = {}
                customContext["preset_name"] =  ShotBrowserEngine.presetsModel.get(
                                    activeScopeIndex,
                                    "nameRole"
                                )

                Future.promise(
                    ShotBrowserEngine.executeQueryJSON(
                        [ShotBrowserEngine.presetsModel.get(activeScopeIndex, "jsonPathRole")],
                        {},
                        custom,
                        customContext
                    )
                ).then(
                    function(json_string) {
                        if(queryCounter == i) {
                            resultsSelectionModel.clear()
                            resultsBaseModel.setResultDataJSON([json_string])
                        }
                        queryRunning -= 1
                    },
                    function() {
                        resultsSelectionModel.clear()
                        resultsBaseModel.setResultDataJSON([])
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
                anchors.leftMargin: 2
                anchors.topMargin: 2
                anchors.rightMargin: rightSpacing ? 0 : 2
                anchors.bottomMargin: 2
            }
        }

        ShotHistoryActionDiv{id: buttonsDiv
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            parentWidth: parent.width - 4
        }
    }
}