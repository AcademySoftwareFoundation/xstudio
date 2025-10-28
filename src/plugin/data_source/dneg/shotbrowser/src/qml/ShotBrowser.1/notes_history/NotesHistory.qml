// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0
import ShotBrowser 1.0

Item{
    id: panel
    anchors.fill: parent

    property bool isPanelEnabled: true
    property var dataModel: results

    property color panelColor: XsStyleSheet.panelBgColor

    property var activeScopeIndex: helpers.qModelIndex()
    property var activeTypeIndex: helpers.qModelIndex()

    // Track the uuid of the media that is currently visible in the Viewport
    property var onScreenMediaUuid: currentPlayhead.mediaUuid
    property var onScreenLogicalFrame: currentPlayhead.logicalFrame

    readonly property string panelType: "NotesHistory"

    property bool isPopout: typeof panels_layout_model_index == "undefined"

    property int queryCounter: 0
    property int queryRunning: 0

    property bool isPaused: false

    onOnScreenMediaUuidChanged: {
        if(visible)
            updateTimer.start()
    }

    property real panelPadding: XsStyleSheet.panelPadding

    XsPreference {
        id: addMode
        path: "/plugin/data_source/shotbrowser/add_mode"
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

    ShotBrowserResultModel {
        id: resultsBaseModel
    }

    ShotBrowserResultFilterModel {
        id: results
        sourceModel: resultsBaseModel
    }


    XsHotkeyArea {
        id: hotkey_area
        anchors.fill: parent
        context: "NoteHistory" + panel
        focus: false
    }

    Keys.forwardTo: hotkey_area

    XsHotkey {
        context: "NoteHistory" + panel
        sequence:  "Ctrl+A"
        name: "Select All"
        description: "Select All"
        onActivated: {
            resultPopup.popupDelegateModel = listDiv.delegateModel
            resultPopup.selectAll()
        }
        componentName: "NoteHistory"
    }

    ItemSelectionModel {
        id: resultsSelectionModel
        model: results
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            setIndexesFromPreferences()
            runQuery()
        }
    }

    function setIndexesFromPreferences() {
        if(ShotBrowserEngine.ready  && !activeScopeIndex.valid && prefs.scope) {
            // from panel.
            let i = getScopeIndex(prefs.scope)
            if(i.valid && activeScopeIndex != i)
                activeScopeIndex = i
        }
        if(ShotBrowserEngine.ready  && !activeTypeIndex.valid && prefs.type) {
            // from panel.
            let i = getTypeIndex(prefs.type)
            if(i.valid && activeTypeIndex != i)
                activeTypeIndex = i
        }
    }

    // get presets node under group
    function getScopeIndex(scope_name) {
        for (const ind of ShotBrowserEngine.presetsModel.noteHistoryScope) {
            if(ShotBrowserEngine.presetsModel.get(ind, "nameRole") == scope_name){
                return ind
            }
        }
        return helpers.qModelIndex()
    }

    function getTypeIndex(type_name) {
        for (const ind of ShotBrowserEngine.presetsModel.noteHistoryType) {
            if(ShotBrowserEngine.presetsModel.get(ind, "nameRole") == type_name){
                return ind
            }
        }
        return helpers.qModelIndex()
    }

    Item {
        // Hold properties that we want to persist between sessions.
        id: prefs
        property string type: ""
        property string scope: ""
        property bool initialised: false

        XsStoredPanelProperties {
            propertyNames: ["type", "scope"]
            onPropertiesInitialised: {
                prefs.initialised = true
                setIndexesFromPreferences()
                runQuery()
            }
        }

    }

    onActiveScopeIndexChanged: {
        if(activeScopeIndex && activeScopeIndex.valid) {
            let i = activeScopeIndex.model.get(activeScopeIndex, "nameRole")
            prefs.scope = i
        }
    }

    onActiveTypeIndexChanged: {
        if(activeTypeIndex && activeTypeIndex.valid) {
            let i = activeTypeIndex.model.get(activeTypeIndex, "nameRole")
            prefs.type = i
        }
    }



    Connections {
        target: ShotBrowserEngine
        function onLiveLinkMetadataChanged() {
            runQuery()
        }
    }

    onIsPanelEnabledChanged: {
        ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
        runQuery()
    }

    function runQuery() {
        if(panel.visible && ShotBrowserEngine.ready && isPanelEnabled && !isPaused && activeScopeIndex.valid && activeTypeIndex.valid) {

            if(onScreenMediaUuid == "{00000000-0000-0000-0000-000000000000}") {
                resultsBaseModel.setResultDataJSON([])
            } else {
                // make sure the results appear in sync.
                queryCounter += 1
                queryRunning += 1
                let i = queryCounter

                let customContext = {}
                customContext["preset_name"] =  ShotBrowserEngine.presetsModel.get(
                                    activeScopeIndex,
                                    "nameRole"
                                ) + " - " + ShotBrowserEngine.presetsModel.get(
                                    activeTypeIndex,
                                    "nameRole"
                                )

                Future.promise(
                    ShotBrowserEngine.executeQueryJSON(
                            [
                                ShotBrowserEngine.presetsModel.get(
                                    activeScopeIndex,
                                    "jsonPathRole"
                                ),
                                ShotBrowserEngine.presetsModel.get(
                                    activeTypeIndex,
                                    "jsonPathRole"
                                )
                            ],
                            {},[],customContext
                        )
                    ).then(function(json_string) {
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
                    })
            }
        }
    }

    function activateScope(clickedIndex){
        if(clickedIndex.valid) {
            activeScopeIndex = clickedIndex
            runQuery()
        }
    }

    function activateType(clickedIndex){
        if(clickedIndex.valid) {
            activeTypeIndex = clickedIndex
            runQuery()
        }
    }

    Component.onCompleted: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setIndexesFromPreferences()
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            isPaused = false
            runQuery()
        }
    }

    onVisibleChanged: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setIndexesFromPreferences()
            ShotBrowserHelpers.updateMetadata(isPanelEnabled, onScreenMediaUuid)
            isPaused = false
            runQuery()
        }
    }

    XsGradientRectangle{ id: backgroundDiv
        anchors.fill: parent
    }

    NotesHistoryResultPopup {
        id: resultPopup
        menu_model_name: "note_history_popup"+resultPopup
        popupSelectionModel: resultsSelectionModel
    }


    ColumnLayout{
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        NotesHistoryTitleDiv{
            titleButtonHeight: (XsStyleSheet.widgetStdHeight + 4)
            Layout.fillWidth: true
            Layout.minimumHeight: (titleButtonHeight * 2) + 1
            Layout.maximumHeight: (titleButtonHeight * 2) + 1
        }

        Rectangle{
            color: panelColor
            Layout.fillWidth: true
            Layout.fillHeight: true

            NotesHistoryListDiv{
                id: listDiv
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.topMargin: 2
                anchors.rightMargin: rightSpacing ? 0 : 2
                anchors.bottomMargin: 2
            }
        }

        NotesHistoryActionDiv{
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            // nasty hack because QML crashes, if I try and do it properly.
            parentWidth: parent.width - 4
        }
    }
}