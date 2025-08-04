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

    Timer {
        id: favouriteTimer
        interval: 1000
        running: false
        repeat: false
        onTriggered: {
            // restore favourites.
            if(prefs.inited && sequenceBaseModel && assetBaseModel && sequenceBaseModel.rowCount() && assetBaseModel.rowCount() ) {
                if(prefs.shotTreeHidden != undefined && projectPref.value in prefs.shotTreeHidden) {
                    sequenceBaseModel.setHidden(prefs.shotTreeHidden[projectPref.value])
                }

                if(prefs.assetTreeHidden != undefined && projectPref.value in prefs.assetTreeHidden) {
                    assetBaseModel.setHidden(prefs.assetTreeHidden[projectPref.value])
                }
            } else {
                favouriteTimer.restart()
            }
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
        property bool showHidden: true
        property bool showHiddenPresets: false
        property bool showUnit: false
        property bool showOnlyFavourites: false
        property bool showStatus: false
        property bool showType: false
        property bool showVisibility: false
        property bool showPresetVisibility: false
        property var hideStatus: ["omt", "na", "del", "omtnto", "omtnwd"]
        property var filterProjects: []
        property var filterProjectStatus: []
        property var filterUnit: {}
        property var filterType: []

        property var shotTreeHidden: {}
        property var assetTreeHidden: {}
        // property var presetHidden: []

        property bool inited: false

        onShowOnlyFavouritesChanged: {
            treeModel.setOnlyShowFavourite(showOnlyFavourites)
            menuModel.setOnlyShowFavourite(showOnlyFavourites)
        }

        // onInitedChanged: {
        //     if(inited) {
        //         ShotBrowserEngine.presetsModel.hidden = prefs.presetHidden
        //     }
        // }

        XsStoredPanelProperties {
            onPropertiesInitialised: prefs.inited = true

            propertyNames: [
                "resultPanelWidth",
                "treePlusResultPanelWidth",
                "treePanelWidth",
                "category",
                "showOnlyFavourites",
                "showHidden",
                "showHiddenPresets",
                "hideEmpty",
                "showUnit",
                "showType",
                // "presetHidden",
                "shotTreeHidden",
                "assetTreeHidden",
                "showVisibility",
                "showPresetVisibility",
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

    // function updatePresetsHidden() {
    //     prefs.presetHidden = ShotBrowserEngine.presetsModel.getHidden()
    // }

    function updateHidden() {
        if(assetMode) {
            let tmp = prefs.assetTreeHidden == undefined  ? {} : prefs.assetTreeHidden
            tmp[projectIndex.model.get(projectIndex,"nameRole")] = assetBaseModel.getHidden()
            prefs.assetTreeHidden = tmp
        } else {
            let tmp = prefs.shotTreeHidden == undefined  ? {} : prefs.shotTreeHidden
            tmp[projectIndex.model.get(projectIndex,"nameRole")] = sequenceBaseModel.getHidden()
            prefs.shotTreeHidden = tmp
        }
    }

    ShotBrowserSequenceFilterModel {
        id: sequenceFilterModel
        sourceModel: sequenceBaseModel
        hideStatus: prefs.hideStatus
        hideEmpty: prefs.hideEmpty
        showHidden: prefs.showHidden
        typeFilter: prefs.filterType
        // unitFilter:
        onUnitFilterChanged: {
            let tmp = prefs.filterUnit == undefined  ? {} : prefs.filterUnit
            tmp[projectIndex.model.get(projectIndex,"nameRole")] = unitFilter
            prefs.filterUnit = tmp
        }
    }

    ShotBrowserSequenceFilterModel {
        id: assetFilterModel
        sourceModel: assetBaseModel
        hideStatus: prefs.hideStatus
        showHidden: prefs.showHidden
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
        showHidden: prefs.showHiddenPresets
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
        showHidden: prefs.showHiddenPresets
        // onlyShowFavourite: true
        filterGroupUserData: "menus"
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

            favouriteTimer.start()
        }
    }

    XsSplitView {
        id: main_split
        anchors.fill: parent

        readonly property int minimumResultWidth: 500
        readonly property int minimumPresetWidth: 250
        readonly property int minimumTreeWidth: 150

        XsSBLeftSection{ id: leftSection
            SplitView.fillHeight: true
            SplitView.minimumWidth: prefs.treePanelWidth + 150
            SplitView.preferredWidth: main_split.width - prefs.treePlusResultPanelWidth
        }

        XsGradientRectangle{
            SplitView.fillHeight: true
            SplitView.fillWidth: true
            SplitView.minimumWidth: main_split.minimumResultWidth

            onWidthChanged: {
                if(SplitView.view.resizing)
                    prefs.treePlusResultPanelWidth = width
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
        // resultsFilteredModel.filterVan = (onDisk == "van")
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
                if(custom.length || ShotBrowserEngine.presetsModel.get(currentPresetIndex, "entityRole") == "Playlists" || ShotBrowserEngine.presetsModel.get(currentPresetIndex, "groupFlagRole").includes("Allow Project Query")) {
                    if(ShotBrowserEngine.presetsModel.get(currentPresetIndex, "groupFlagRole").includes("Ignore Tree Selection"))
                        custom = []

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