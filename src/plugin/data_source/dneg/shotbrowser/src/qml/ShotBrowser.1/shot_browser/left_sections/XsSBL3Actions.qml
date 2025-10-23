// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

Rectangle{
    color: "transparent" //panelColor

    property real itemHeight: XsStyleSheet.widgetStdHeight

    function executeQuery(queryIndex, action) {
        if(queryIndex.valid) {
            // clear current result set
            quickResults.setResultDataJSON([])

            let pi = ShotBrowserEngine.presetsModel.termModel("Project").get(projectIndex, "idRole")
            let custom = []

            let seqsel = []

            if(assetMode) {
                for(let i=0;i<assetSelectionModel.selectedIndexes.length; i++){
                    let tindex = assetSelectionModel.selectedIndexes[i]
                    let findex = tindex.model.mapToModel(tindex)
                    seqsel.push(findex.model.mapToSource(findex))
                }
            } else {
                for(let i=0;i<sequenceSelectionModel.selectedIndexes.length; i++){
                    let tindex = sequenceSelectionModel.selectedIndexes[i]
                    let findex = tindex.model.mapToModel(tindex)
                    seqsel.push(findex.model.mapToSource(findex))
                }
            }


            for(let i=0;i<seqsel.length;i++) {
                let t = seqsel[i].model.get(seqsel[i],"typeRole")
                if(t == "Shot") {
                    custom.push({
                        "enabled": true,
                        "type": "term",
                        "term": "Shot",
                        "value": seqsel[i].model.get(seqsel[i],"nameRole")
                    })
                } else if( t == "Sequence") {
                    custom.push({
                        "enabled": true,
                        "type": "term",
                        "term": "Sequence",
                        "value": seqsel[i].model.get(seqsel[i],"nameRole")
                    })
                } else if( t == "Episode") {
                    custom.push({
                        "enabled": true,
                        "type": "term",
                        "term": "Episode",
                        "value": seqsel[i].model.get(seqsel[i],"nameRole")
                    })
                } else if( t == "Asset") {
                    custom.push({
                        "enabled": true,
                        "type": "term",
                        "term": "Asset",
                        "value": seqsel[i].model.get(seqsel[i],"nameRole")
                    })
                }
            }


            // only run, if selection in tree.
            if(custom.length) {

                let customContext = {}
                customContext["preset_name"] = ShotBrowserEngine.presetsModel.get(queryIndex, "nameRole")
                customContext["project_name"] = projectPref.value

                let result_json = []
                let result_count = custom.length

                for(let i = 0; i< result_count;i++) {
                    Future.promise(
                        ShotBrowserEngine.executeProjectQueryJSON(
                            [ShotBrowserEngine.presetsModel.get(queryIndex, "jsonPathRole")], pi, {}, [custom[i]], customContext)
                        ).then(function(json_string) {
                            result_json[i] = json_string
                            result_count -= 1
                            if(!result_count) {
                                quickResults.setResultDataJSON(result_json)
                                let indexes = []
                                for(let j=0;j<quickResults.rowCount();j++) {
                                    indexes.push(quickResults.index(j,0))
                                }
                                if(action == "playlist") {
                                    ShotBrowserHelpers.addToCurrent(indexes, false, addMode.value)
                                } else if(action == "sequence") {
                                    let seq_map = {}

                                    for(let j=0;j<indexes.length;j++) {
                                        let seq = quickResults.get(indexes[j], "sequenceRole")
                                        if(seq_map[seq] === undefined)
                                            seq_map[seq] = [indexes[j]]
                                        else
                                            seq_map[seq].push(indexes[j])
                                    }
                                    for(let key in seq_map) {
                                        ShotBrowserHelpers.addToPlaylist(seq_map[key], null, null, key, ShotBrowserHelpers.conformToNewSequenceCallback)
                                    }
                                }
                            }
                        },
                        function() {
                            result_json[i] = null
                            result_count -= 1
                            if(!result_count) {
                                quickResults.setResultDataJSON(result_json)
                                let indexes = []
                                for(let j=0;j<quickResults.rowCount();j++)
                                    indexes.push(quickResults.index(j,0))

                                if(action == "playlist") {
                                    ShotBrowserHelpers.addToCurrent(indexes, false, addMode.value)
                                } else if(action == "sequence") {

                                }
                            }
                        })
                    }
            }
        }
    }

    ShotBrowserResultModel {
        id: quickResults
    }

    function refreshModel() {
        quickModel.clear()
        for(let i=0;i<ShotBrowserEngine.presetsModel.quickLoad.length; i++) {
            quickModel.append(
                {
                    "nameRole": ShotBrowserEngine.presetsModel.get(ShotBrowserEngine.presetsModel.quickLoad[i],"nameRole")
                }
            )
        }
    }

    Connections {
        target: ShotBrowserEngine.presetsModel
        function onQuickLoadChanged() {
            refreshModel()
        }
    }

    Component.onCompleted: {
        refreshModel()
    }

    ListModel {
        id: quickModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: buttonSpacing
        anchors.leftMargin: buttonSpacing
        spacing: buttonSpacing*4

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height/2
            spacing: buttonSpacing*4

            XsText{
                text: "Quick Load:"
                elide: Text.ElideRight
                Layout.fillHeight: true
            }

            XsComboBox {
                Layout.fillWidth: true
                Layout.fillHeight: true

                id: quickCombo
                model: quickModel
                textRole: "nameRole"
                currentIndex: 0
                onActivated: prefs.quickLoad = currentText
                onCountChanged: currentIndex = find(prefs.quickLoad)
            }
        }

        RowLayout {
            spacing: buttonSpacing*4
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height/2

            XsPrimaryButton{
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Add"
                onClicked: {
                    if(quickCombo.currentIndex != -1) {
                        executeQuery(
                            ShotBrowserEngine.presetsModel.quickLoad[quickCombo.currentIndex],
                             "playlist"
                        )
                    }
                }
            }

            XsPrimaryButton{
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Conform Selected"
                onClicked: {
                    if(quickCombo.currentIndex != -1)
                        executeQuery(
                            ShotBrowserEngine.presetsModel.quickLoad[quickCombo.currentIndex],
                             "sequence"
                        )
                }
            }
        }
    }
}