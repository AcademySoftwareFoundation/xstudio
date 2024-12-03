// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

Rectangle{
    color: "transparent" //panelColor

    property real itemHeight: XsStyleSheet.widgetStdHeight

    Component.onCompleted: {
        populateModels()
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            populateModels()
        }
    }

    // make sure index is updated
    XsModelProperty {
        index: quickModel.rootIndex

        onIndexChanged: {
            if(!index.valid) {
                // clear list views
                quickCombo.model = []
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
            if(ShotBrowserEngine.ready && ShotBrowserEngine.presetsModel.rowCount()) {
                let sourceInd = ShotBrowserEngine.presetsModel.searchRecursive(
                    "137aa66a-87e2-4c53-b304-44bd7ff9f755", "idRole", ShotBrowserEngine.presetsModel.index(-1, -1), 0, 1
                )
                let ind = filterModel.mapFromSource(sourceInd)

                if(ind.valid) {
                    quickModel.rootIndex = ind
                    quickCombo.model = quickModel
                } else {
                    quickCombo.model = []
                    populateTimer.start()
                }
            } else {
                populateTimer.start()
            }
        }
    }

    function populateModels() {
        populateTimer.start()
    }

    function executeQuery(queryIndex, action) {
        if(queryIndex.valid) {
            // clear current result set
            quickResults.setResultData([])

            let pi = ShotBrowserEngine.presetsModel.termModel("Project").get(projectIndex, "idRole")
            let custom = []

            let seqsel = []
            for(let i=0;i<sequenceSelectionModel.selectedIndexes.length; i++){
                let tindex = sequenceSelectionModel.selectedIndexes[i]
                let findex = tindex.model.mapToModel(tindex)
                seqsel.push(findex.model.mapToSource(findex))
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
                }
            }


            // only run, if selection in tree.
            if(custom.length) {

                let result_json = []
                let result_count = custom.length

                for(let i = 0; i< result_count;i++) {
                    Future.promise(
                        ShotBrowserEngine.executeProjectQuery(
                            [ShotBrowserEngine.presetsModel.get(queryIndex, "jsonPathRole")], pi, {}, [custom[i]])
                        ).then(function(json_string) {
                            result_json[i] = json_string
                            result_count -= 1
                            if(!result_count) {
                                quickResults.setResultData(result_json)
                                let indexes = []
                                for(let j=0;j<quickResults.rowCount();j++) {
                                    indexes.push(quickResults.index(j,0))
                                }
                                if(action == "playlist") {
                                    ShotBrowserHelpers.addToCurrent(indexes, false)
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
                            result_json[i] = ""
                            result_count -= 1
                            if(!result_count) {
                                quickResults.setResultData(result_json)
                                let indexes = []
                                for(let j=0;j<quickResults.rowCount();j++)
                                    indexes.push(quickResults.index(j,0))

                                if(action == "playlist") {
                                    ShotBrowserHelpers.addToCurrent(indexes, false)
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

    ShotBrowserPresetFilterModel {
        id: filterModel
        showHidden: false
        onlyShowFavourite: true
        sourceModel: ShotBrowserEngine.presetsModel
    }

    DelegateModel {
        id: quickModel
        property var notifyModel: filterModel
        rootIndex: helpers.qModelIndex()
        model: notifyModel
        delegate:  quickCombo.delegate
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
                    if(quickCombo.currentIndex != -1)
                        executeQuery(
                            quickModel.notifyModel.mapToSource(quickModel.modelIndex(quickCombo.currentIndex)),
                             "playlist"
                        )
                }
            }

            XsPrimaryButton{
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Conform To New Sequence"
                onClicked: {
                    if(quickCombo.currentIndex != -1)
                        executeQuery(
                            quickModel.notifyModel.mapToSource(quickModel.modelIndex(quickCombo.currentIndex)),
                             "sequence"
                        )
                }
            }
        }
    }
}