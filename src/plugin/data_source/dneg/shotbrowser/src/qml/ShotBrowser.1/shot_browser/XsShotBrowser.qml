// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import ShotBrowser 1.0

Item{
    id: panel
    anchors.fill: parent

    property string resultViewTitle: ""

    property alias currentCategory: prefs.category
    property bool isGroupedByLatest: false

    property real buttonSpacing: 1
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight + 4
    property real panelPadding: XsStyleSheet.panelPadding
    property color panelColor: XsStyleSheet.panelBgColor

    property bool queryRunning: queryRunningCount > 0

    property bool assetMode: false

    property var onScreenMediaUuid: currentPlayhead.mediaUuid
    property var onScreenLogicalFrame: currentPlayhead.logicalFrame

    property int projectId: -1
    property var projectIndex: null
    property var sequenceBaseModel: null
    property var assetBaseModel: null
    property var currentPresetIndex: ShotBrowserEngine.presetsModel.index(-1,-1)

    property var categoryPreset: (new Map())

    property bool sequenceTreeLiveLink: false
    property bool sequenceTreeShowPresets: true

    property bool isPopout: typeof panels_layout_model_index == "undefined"

    property alias sortByNaturalOrder: resultsFilteredModel.sortByNaturalOrder
    property alias sortByCreationDate: resultsFilteredModel.sortByCreationDate
    property alias sortByShotName: resultsFilteredModel.sortByShotName
    property alias sortInAscending: resultsFilteredModel.sortInAscending
    property alias pipeStep: resultsFilteredModel.filterPipeStep
    property alias nameFilter: resultsFilteredModel.filterName

    readonly property string panelType: "ShotBrowser"

    property string onDisk: ""

    property int queryCounter: 0
    property int queryRunningCount: 0

    property bool isPaused: false

    onOnScreenMediaUuidChanged: {if(visible) updateTimer.start()}

    onCurrentPresetIndexChanged: {
        if(currentPresetIndex.valid)
            categoryPreset[currentCategory] = helpers.makePersistent(currentPresetIndex)
    }

    onCurrentCategoryChanged: {
        if(currentCategory in categoryPreset && categoryPreset[currentCategory].valid) {
            activatePreset(categoryPreset[currentCategory])
            presetsSelectionModel.select(categoryPreset[currentCategory], ItemSelectionModel.ClearAndSelect)
        }
    }

    onOnScreenLogicalFrameChanged: {
        if(updateTimer.running)
            updateTimer.restart()
        if(!isPaused && (currentCategory == "Menus" && currentPresetIndex.valid) || (currentCategory == "Tree" && sequenceTreeLiveLink)) {
            isPaused = true
        }
    }

    // why was this commented out ?
    MouseArea {
        anchors.fill: parent
        onClicked: forceActiveFocus(panel)
    }

    Timer {
        id: updateTimer
        interval: 500
        running: false
        repeat: false
        onTriggered: {
            isPaused = false
            updateMetaData()
        }
    }

    Clipboard {
        id: clipboard
    }

    function setProjectIndex(force = false) {
        if(ShotBrowserEngine.ready && (force || (projectIndex == null || !projectIndex.valid))) {
            let pi = getProjectIndexFromName(projectPref.value)
            if(pi.valid)
                projectIndex = pi
        }
    }

    function updateMetaData() {
        if(visible) {
            if((currentCategory == "Menus" && currentPresetIndex.valid) || (currentCategory == "Tree" && sequenceTreeLiveLink))
                return ShotBrowserHelpers.updateMetadata(true, onScreenMediaUuid)
        }
        return false
    }

    function updateTreeSelection() {
        if(visible) {
            if(currentCategory == "Tree" && sequenceTreeLiveLink) {
                // update tree selection
                let pname = ShotBrowserEngine.getProjectFromMetadata()
                if(!projectIndex.valid || projectIndex.model.get(projectIndex,"nameRole") != pname) {
                    projectIndex = getProjectIndexFromName(pname)
                }
                let entity = ShotBrowserEngine.getEntityFromMetadata()

                if(entity != undefined) {
                    if(assetMode && entity.type == "Asset") {
                        let bi = assetBaseModel.searchRecursive(entity.id, "idRole")
                        if(bi.valid) {
                            let fi = assetFilterModel.mapFromSource(bi)
                            if(fi.valid) {
                                let parents = helpers.getParentIndexes([fi])
                                for(let i = 0; i< parents.length; i++){
                                    if(parents[i].valid) {
                                        // find in tree.. Order ?
                                        let ti = assetTreeModel.mapFromModel(parents[i])
                                        if(ti.valid) {
                                            assetTreeModel.expandRow(ti.row)
                                        }
                                    }
                                }
                                // should now be visible
                                let ti = assetTreeModel.mapFromModel(fi)
                                assetSelectionModel.select(ti, ItemSelectionModel.ClearAndSelect)
                            }
                        }

                    } else if(!assetMode && entity.type != "Asset"){
                        let bi = sequenceBaseModel.searchRecursive(entity.name)
                        if(bi.valid) {
                            let fi = sequenceFilterModel.mapFromSource(bi)
                            if(fi.valid) {
                                let parents = helpers.getParentIndexes([fi])
                                for(let i = 0; i< parents.length; i++){
                                    if(parents[i].valid) {
                                        // find in tree.. Order ?
                                        let ti = sequenceTreeModel.mapFromModel(parents[i])
                                        if(ti.valid) {
                                            sequenceTreeModel.expandRow(ti.row)
                                        }
                                    }
                                }
                                // should now be visible
                                let ti = sequenceTreeModel.mapFromModel(fi)
                                sequenceSelectionModel.select(ti, ItemSelectionModel.ClearAndSelect)
                            }
                        }
                    }
                }
            }
        }
    }

    onSequenceTreeLiveLinkChanged: {
        if(sequenceTreeLiveLink) {
            updateMetaData()
            updateTreeSelection()
        }
    }

    Connections {
        target: ShotBrowserEngine
        function onLiveLinkMetadataChanged() {
            if(panel.visible && !isPaused) {
                updateTreeSelection()

                if(currentCategory == "Menus" && currentPresetIndex.valid) {
                    executeQuery()
                }
            }
        }
    }

    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {setProjectIndex()}
    }

    Connections {
        target: ShotBrowserEngine.presetsModel
        function onPresetChanged(index) {
            if(currentPresetIndex == index) {
                executeQuery()
            }
        }
    }

    function getProjectIndexFromId(project_id) {
        let m = ShotBrowserEngine.presetsModel.termModel("Project")
        return m.searchRecursive(project_id, "idRole")
    }

    function getProjectIndexFromName(project_name) {
        let m = ShotBrowserEngine.presetsModel.termModel("Project")
        return m.searchRecursive(project_name, "nameRole")
    }

    XsPreference {
        id: projectPref
        path: "/plugin/data_source/shotbrowser/browser/project"
        onValueChanged: setProjectIndex(true)
    }

    XsPreference {
        id: addAfterSelection
        path: "/plugin/data_source/shotbrowser/add_after_selection"
    }

    Item {
        // Hold properties that we want to persist between sessions.
        id: prefs
        property int resultPanelWidth: 600
        property int treePlusResultPanelWidth: 600
        property int treePanelWidth: 200
        property string category: "Tree"
        property string quickLoad: ""
        property bool hideEmpty: false
        property bool showUnit: false
        property bool showOnlyFavourites: false
        property bool showStatus: false
        property bool showType: false
        property var hideStatus: ["omt", "na", "del", "omtnto", "omtnwd"]
        property var filterProjects: []
        property var filterProjectStatus: []
        property var filterUnit: (new Map())
        property var filterType: []

        onShowOnlyFavouritesChanged: {
            treeModel.setOnlyShowFavourite(showOnlyFavourites)
            menuModel.setOnlyShowFavourite(showOnlyFavourites)
            recentModel.setOnlyShowFavourite(showOnlyFavourites)
        }

        XsStoredPanelProperties {

            propertyNames: [
                "resultPanelWidth",
                "treePlusResultPanelWidth",
                "treePanelWidth",
                "category",
                "showOnlyFavourites",
                "hideEmpty",
                "showUnit",
                "showType",
                "hideStatus",
                "showStatus",
                "quickLoad",
                "filterUnit",
                "filterType",
                "filterProjects",
                "filterProjectStatus"
            ]
        }

    }

    ShotBrowserSequenceFilterModel {
        id: sequenceFilterModel
        sourceModel: sequenceBaseModel
        hideStatus: prefs.hideStatus
        hideEmpty: prefs.hideEmpty
        typeFilter: prefs.filterType
        // unitFilter:
        onUnitFilterChanged: {
            let tmp = prefs.filterUnit
            tmp[projectIndex.model.get(projectIndex,"nameRole")] = unitFilter
            prefs.filterUnit = tmp
        }
    }

    ShotBrowserSequenceFilterModel {
        id: assetFilterModel
        sourceModel: assetBaseModel
        hideStatus: prefs.hideStatus
    }

    QTreeModelToTableModel {
        id: sequenceTreeModel
        model: sequenceFilterModel
    }

    QTreeModelToTableModel {
        id: assetTreeModel
        model: assetFilterModel
    }

    ItemSelectionModel {
        id: sequenceSelectionModel
        model: sequenceTreeModel
        onSelectionChanged: executeQuery()
    }

    ItemSelectionModel {
        id: assetSelectionModel
        model: assetTreeModel
        onSelectionChanged: executeQuery()
    }

    ItemSelectionModel {
        id: presetsExpandedModel
        model: ShotBrowserEngine.presetsModel
    }

    ItemSelectionModel {
        id: presetsSelectionModel
        model: ShotBrowserEngine.presetsModel
    }

    ShotBrowserResultModel {
        id: resultsBaseModel
        isGrouped: true
    }

    ShotBrowserResultFilterModel {
        id: resultsFilteredModel
        sourceModel: resultsBaseModel
    }

    QTreeModelToTableModel {
        id: results
        model: resultsFilteredModel
    }

    ItemSelectionModel {
        id: resultsSelectionModel
        model: results
    }

    ShotHistoryResultPopup {
        id: versionResultPopup
        menu_model_name: "version_shot_browser_popup"+versionResultPopup
        popupSelectionModel: resultsSelectionModel
    }

    NotesHistoryResultPopup {
        id: noteResultPopup
        menu_model_name: "note_shot_browser_popup"+noteResultPopup
        popupSelectionModel: resultsSelectionModel
    }

    XsSBRPlaylistResultPopup {
        id: playlistResultPopup
        menu_model_name: "playlist_shot_browser_popup"+playlistResultPopup
        popupSelectionModel: resultsSelectionModel
    }

    ShotBrowserPresetFilterModel {
        id: treeModel
        showHidden: false
        // onlyShowFavourite: true
        filterGroupUserData: "tree"
        sourceModel: ShotBrowserEngine.presetsModel
    }

    ShotBrowserPresetFilterModel {
        id: treeButtonModel
        showHidden: false
        onlyShowFavourite: true
        onlyShowPresets: true
        ignoreToolbar: true
        filterGroupUserData: "tree"
        sourceModel: buttonModelBase

        onModelReset: {
            if(!currentPresetIndex.valid) {
                let i = treeButtonModel.index(0,0)
                if(i.valid) {
                    i = treeButtonModel.mapToSource(i)
                    currentPresetIndex = i.model.mapToModel(i)
                }
            }
        }
        onRowsInserted: {
            if(!currentPresetIndex.valid) {
                let i = treeButtonModel.index(0,0)
                if(i.valid) {
                    i = treeButtonModel.mapToSource(i)
                    currentPresetIndex = i.model.mapToModel(i)
                }
            }
        }
    }

    ShotBrowserPresetFilterModel {
        id: recentButtonModel
        showHidden: false
        onlyShowFavourite: true
        onlyShowPresets: true
        ignoreToolbar: true
        filterGroupUserData: "recent"
        sourceModel: buttonModelBase
    }

    ShotBrowserPresetFilterModel {
        id: menuButtonModel
        showHidden: false
        onlyShowFavourite: true
        onlyShowPresets: true
        ignoreToolbar: true
        filterGroupUserData: "menu"
        sourceModel: buttonModelBase
    }

    QTreeModelToTableModel {
        id: buttonModelBase
        model: ShotBrowserEngine.presetsModel
        onCountChanged: expandAll(2)
    }

    ShotBrowserPresetFilterModel {
        id: menuModel
        showHidden: false
        // onlyShowFavourite: true
        filterGroupUserData: "menus"
        sourceModel: ShotBrowserEngine.presetsModel
    }

    ShotBrowserPresetFilterModel {
        id: recentModel
        showHidden: false
        // onlyShowFavourite: true
        filterGroupUserData: "recent"
        sourceModel: ShotBrowserEngine.presetsModel
    }

    onProjectIndexChanged: {
        if(projectIndex && projectIndex.valid) {
            let m = ShotBrowserEngine.presetsModel.termModel("Project")
            let i = m.get(projectIndex, "idRole")
            ShotBrowserEngine.cacheProject(i)
            sequenceBaseModel = ShotBrowserEngine.sequenceTreeModel(i)
            assetBaseModel = ShotBrowserEngine.assetTreeModel(i)

            projectPref.value = m.get(projectIndex, "nameRole")
            projectId = i
        }
    }

    XsSplitView {
        id: main_split
        anchors.fill: parent

        property bool treePlusActive: currentCategory == "Tree" && sequenceTreeShowPresets

        readonly property int minimumResultWidth: 600
        readonly property int minimumPresetWidth: 330
        readonly property int minimumTreeWidth: 230

        XsSBLeftSection{ id: leftSection
            SplitView.fillHeight: true
            // SplitView.fillWidth: true
            SplitView.minimumWidth: (
                main_split.treePlusActive ?
                prefs.treePanelWidth + 150 :
                main_split.minimumPresetWidth
            )
            SplitView.preferredWidth: (main_split.treePlusActive ? main_split.width - prefs.treePlusResultPanelWidth : main_split.width -prefs.resultPanelWidth)
        }

        XsGradientRectangle{
            SplitView.fillHeight: true
            SplitView.fillWidth: true
            // SplitView.preferredWidth: (main_split.treePlusActive ? prefs.treePlusResultPanelWidth : prefs.resultPanelWidth)
            SplitView.minimumWidth: main_split.minimumResultWidth

            onWidthChanged: {
                if(SplitView.view.resizing) {
                    if(main_split.treePlusActive) {
                        prefs.treePlusResultPanelWidth = width
                    } else {
                        prefs.resultPanelWidth = width
                    }
                }
            }

            XsSBRightSection {

                anchors.fill: parent
                anchors.margins: 4
                id: right_section

            }
        }
    }

    onOnDiskChanged: {
        resultsFilteredModel.filterChn = (onDisk == "chn")
        resultsFilteredModel.filterLon = (onDisk == "lon")
        resultsFilteredModel.filterMtl = (onDisk == "mtl")
        resultsFilteredModel.filterMum = (onDisk == "mum")
        resultsFilteredModel.filterVan = (onDisk == "van")
        resultsFilteredModel.filterSyd = (onDisk == "syd")
    }

    function executeQuery() {
        if(currentPresetIndex && currentPresetIndex.valid) {
            let customContext = {}
            customContext["project_name"] = projectPref.value
            customContext["preset_name"] = ShotBrowserEngine.presetsModel.get(currentPresetIndex, "nameRole")

            resultsSelectionModel.clear()

            // pipeStep
            if(currentCategory == "Menus") {
                queryCounter += 1
                queryRunningCount += 1
                let i = queryCounter

                Future.promise(
                    ShotBrowserEngine.executeQueryJSON(
                        [ShotBrowserEngine.presetsModel.get(currentPresetIndex, "jsonPathRole")], {}, [], customContext)
                    ).then(function(json_string) {
                        // console.log(json_string)
                        if(queryCounter == i) {
                            resultsSelectionModel.clear()
                            resultsBaseModel.setResultDataJSON([json_string])
                        }
                        queryRunningCount -= 1
                    },
                    function() {
                        resultsBaseModel.setResultDataJSON([])
                        queryRunningCount -= 1
                    })
            } else if(currentCategory == "Recent") {
                queryCounter += 1
                queryRunningCount += 1

                let i = queryCounter

                Future.promise(
                    ShotBrowserEngine.executeProjectQueryJSON(
                        [ShotBrowserEngine.presetsModel.get(currentPresetIndex, "jsonPathRole")], projectId, {}, [], customContext)
                    ).then(function(json_string) {
                        // console.log(json_string)
                        if(queryCounter == i) {
                            resultsSelectionModel.clear()
                            resultsBaseModel.setResultDataJSON([json_string])
                        }
                        queryRunningCount -= 1
                    },
                    function() {
                        resultsBaseModel.setResultDataJSON([])
                        queryRunningCount -= 1
                    })
            } else {
                let custom = []
                // convert to base model.
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
                            "value": seqsel[i].model.get(seqsel[i],"assetNameRole")
                        })
                    }
                }

                // only run, if selection in tree.
                if(custom.length || ShotBrowserEngine.presetsModel.get(currentPresetIndex, "entityRole") == "Playlists") {
                    queryCounter += 1
                    queryRunningCount += 1

                    let i = queryCounter
                    Future.promise(
                        ShotBrowserEngine.executeProjectQueryJSON(
                            [ShotBrowserEngine.presetsModel.get(currentPresetIndex, "jsonPathRole")], projectId, {}, custom, customContext)
                        ).then(function(json_string) {
                            // console.log(json_string)
                            if(queryCounter == i) {
                                resultsSelectionModel.clear()
                                resultsBaseModel.setResultDataJSON([json_string])
                            }
                            queryRunningCount -= 1
                        },
                        function() {
                            resultsBaseModel.setResultDataJSON([])
                            queryRunningCount -= 1
                        })
                } else {
                    resultsBaseModel.setResultDataJSON([])
                }
            }
        }
    }

    function activatePreset(clickedIndex){
        // map to preset model..
        currentPresetIndex = clickedIndex
        if(projectIndex && projectIndex.valid) {
            if(currentPresetIndex.valid) {
                executeQuery()
            }
        }
    }

    Component.onCompleted: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setProjectIndex()
        }
    }

    onVisibleChanged: {
        if(visible) {
            ShotBrowserEngine.connected = true
            setProjectIndex()
            updateMetaData()
        }
    }

}