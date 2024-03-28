// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import QuickFuture 1.0
import QuickPromise 1.0

import Shotgun 1.0
import xStudio 1.1
import xstudio.qml.clipboard 1.0

Rectangle{ id: leftDiv
    property var projectModel: null
    property alias projectCurrentIndex: projComboBox.currentIndex

    property bool liveLinkProjectChange: false
    property var authenticateFunc: null

    property var treeMode: null

    property var authorModel: null
    property var siteModel: null
    property var departmentModel: null
    property var noteTypeModel: null
    property var playlistTypeModel: null
    property var productionStatusModel: null
    property var pipelineStatusModel: null

    property var boolModel: null
    property var resultLimitModel: null
    property var reviewLocationModel: null
    property var referenceTagModel: null
    property var orderByModel: null
    property var primaryLocationModel: null
    property var lookbackModel: null
    property var stepModel: null
    property var onDiskModel: null
    property var twigTypeCodeModel: null
    property var shotStatusModel: null

    property var sequenceModel: null
    property var sequenceModelFunc: null

    property var sequenceTreeModel: null
    property var sequenceTreeModelFunc: null

    property var shotModel: null
    property var shotModelFunc: null

    property var customEntity24Model: null
    property var customEntity24ModelFunc: null

    property var shotSearchFilterModel: null
    property var shotSearchFilterModelFunc: null

    property var playlistModel: null
    property var playlistModelFunc: null

    property var filterViewModel: null

    property alias presetSelectionModel: presetsDiv.presetSelectionModel
    property alias presetTreeSelectionModel: presetsMiniDiv.presetSelectionModel

    property var searchPresetsViewModel: null
    property var searchTreePresetsViewModel: null
    property var searchQueryViewModel: null

    property var shotPresetsModel: null
    property var shotTreePresetsModel: null
    property var playlistPresetsModel: null
    property var editPresetsModel: null
    property var referencePresetsModel: null
    property var notePresetsModel: null
    property var noteTreePresetsModel: null
    property var mediaActionPresetsModel: null

    property var shotFilterModel: null
    property var playlistFilterModel: null
    property var editFilterModel: null
    property var referenceFilterModel: null
    property var noteFilterModel: null
    property var mediaActionFilterModel: null

    property var executeQueryFunc: null
    property var mergeQueriesFunc: null

    // dynamic filters from right panel
    property var pipelineStepFilterIndex: null
    property var onDiskFilterIndex: null

    property var lastQuery: null
    property var categoryLastMode: {"Versions": "Versions Tree", "Notes": "Notes"}

    property var presetsModel: presetsDiv.presetsModel
    property var searchPresetsView: presetsDiv.searchPresetsView
    property alias presetsDiv: presetsDiv
    property var clearFilter: null


    color: "transparent"

    property var shotFilters: [
"Author",
"Completion Location",
"Disable Global",
"Exclude Shot Status",
"Filter",
"Flag Media",
"Has Notes",
"Is Hero",
"Latest Version",
"Lookback",
"Newer Version",
"Older Version",
"On Disk",
"Order By",
"Pipeline Status",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Production Status",
"Reference Tag",
"Reference Tags",
"Result Limit",
"Sent To Client",
"Sent To Dailies",
"Sequence",
"Shot Status",
"Shot",
"Tag",
"Twig Name",
"Twig Type"
    ]
    property var mediaActionFilters: [
"Author",
"Completion Location",
"Disable Global",
"Exclude Shot Status",
"Filter",
"Flag Media",
"Has Notes",
"Is Hero",
"Latest Version",
"Lookback",
"Newer Version",
"Older Version",
"On Disk",
"Order By",
"Pipeline Status",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Production Status",
"Reference Tag",
"Reference Tags",
"Result Limit",
"Sent To Client",
"Sent To Dailies",
"Sequence",
"Shot Status",
"Shot",
"Tag",
"Twig Name",
"Twig Type"
    ]
    property var playlistFilters: [
"Author",
"Department",
"Disable Global",
"Exclude Shot Status",
"Filter",
"Flag Media",
"Has Contents",
"Has Notes",
"Lookback",
"Order By",
"Playlist Type",
"Preferred Audio",
"Preferred Visual",
"Review Location",
"Result Limit",
"Shot Status",
"Site",
"Tag",
"Unit"
    ]
    property var referenceFilters: [
"Author",
"Completion Location",
"Disable Global",
"Exclude Shot Status",
"Filter",
"Flag Media",
"Has Notes",
"Is Hero",
"Latest Version",
"Lookback",
"On Disk",
"Order By",
"Pipeline Status",
"Pipeline Step",
"Preferred Audio",
"Preferred Visual",
"Production Status",
"Reference Tag",
"Reference Tags",
"Result Limit",
"Sent To Client",
"Sent To Dailies",
"Sequence",
"Shot Status",
"Shot",
"Tag",
"Twig Name",
"Twig Type"
    ]
    property var editFilters: [
    ]
    property var noteFilters: [
"Author",
"Disable Global",
"Exclude Shot Status",
"Filter",
"Flag Media",
"Lookback",
"Newer Version",
"Note Type",
"Older Version",
"Order By",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Recipient",
"Result Limit",
"Sequence",
"Shot Status",
"Shot",
"Tag",
"Twig Name",
"Twig Type",
"Version Name"
    ]

    onTreeModeChanged:{
        tabBar.currentIndex = treeMode ? 1 : 0
        if(treeMode) {
            presetsModel = presetsMiniDiv.presetsModel
            searchPresetsView = presetsMiniDiv.searchPresetsView
        } else {
            presetsModel = presetsDiv.presetsModel
            searchPresetsView = presetsDiv.searchPresetsView
        }

        if( !["Versions","Notes"].includes(currentCategoryClass) ) tabBar.currentIndex = 0
    }

    property bool isCollapsed: false
    onIsCollapsedChanged:{

        if(isCollapsed) {
            if(presetsModel){
                for(let i=0; i<presetsModel.count; i++) presetsModel.set(i, false, "expandedRole")
            }
        }
        else{
            if(presetsModel){
                for(let i=0; i<presetsModel.count; i++)
                if(i == searchPresetsView.currentIndex) presetsModel.set(i, true, "expandedRole")
            }
        }
    }

    property real collapsedWidth: itemHeight + (framePadding *3)

    SplitView.minimumWidth: isCollapsed? collapsedWidth : minimumLeftSplitViewWidth
    SplitView.preferredWidth:  SplitView.minimumWidth
    SplitView.maximumWidth:  isCollapsed? collapsedWidth : parent.width - minimumLeftSplitViewWidth
    SplitView.minimumHeight: parent.height

    // MouseArea{ id: leftDivMArea; anchors.fill: parent; onPressed: focus= true; onReleased: focus= false;}

    signal busyQuery(queryIndex: int, isBusy: bool)
    signal projectChanged(project_id: int)
    signal undo()
    signal redo()
    signal snapshotGlobals()
    signal snapshotPresets()

    Clipboard {
        id: clipboard
    }

    Timer {
        id: launch_query
        interval: 100
        running: false
        repeat: false
        onTriggered: executeQueryReal()
    }

    function executeQuery() {
        // console.log("executeQuery()")
        if(!launch_query.running)
            launch_query.start()
    }

    XsErrorMessage {
        id: error_dialog
        title: "Shot Browser Error"
        width: 200
        height: 100
    }

    onPipelineStepFilterIndexChanged: {
        if(lastQuery) {
            if(JSON.stringify(lastQuery) !== JSON.stringify(buildQuery())) {
                executeQuery()
            }
        }
    }

    onOnDiskFilterIndexChanged: {
        if(lastQuery) {
            if(JSON.stringify(lastQuery) !== JSON.stringify(buildQuery())) {
                executeQuery()
            }
        }
    }

    function buildQuery() {
        let override  = []
        let query = null

        if(presetsModel &&  filterViewModel && presetsModel.model.activePreset != -1) {

            if(pipelineStepFilterIndex != undefined && pipelineStepFilterIndex != -1) {
                override.push({"term": "Pipeline Step",  "value": stepModel.get(pipelineStepFilterIndex, "nameRole"), "enabled": true})
            }

            if(onDiskFilterIndex != undefined && onDiskFilterIndex != -1) {
                 override.push({"term": "On Disk",  "value": onDiskModel.get(onDiskFilterIndex, "nameRole"), "enabled": true})
            }

            query = mergeQueriesFunc(presetsModel.get(presetsModel.model.activePreset, "jsonRole"), filterViewModel.get(0, "jsonRole"))

            if(override.length) {
                query = mergeQueriesFunc(query, {"queries":override})
            }
        }

        return query
    }

    function executeQueryReal() {
        // console.log("executeQueryReal")
        if(currentCategory) {
            let query = buildQuery()
            if(query !== null) {
                lastQuery = query

                let preset_index = presetsModel.model.activePreset
                busyQuery(preset_index, true)

                Future.promise(
                    executeQueryFunc(currentCategory, projectModel.get(projComboBox.currentIndex, "idRole"), query, true)
                ).then(function(json_string) {
                    busyQuery(preset_index, false)

                    try {
                        JSON.parse(json_string)
                    } catch(err) {
                        error_dialog.text = json_string
                        error_dialog.open()
                    }
                },
                function() {
                    busyQuery(preset_index, false)
                })
            } else {
                // console.log("Bad query")
            }
        }
    }

    function clearResults() {
        rightDiv.searchResultsViewModel.sourceModel.clear()
        rightDiv.clearFilter()
        presetSelectionModel.clear()
        presetTreeSelectionModel.clear()
        searchPresetsView.currentIndex = -1
    }

    function updateVersionHistory(shot_seq, is_shot, twigname, pstep, twigtype) {
        let mymodel = searchTreePresetsViewModel
        let preset_id = mymodel.search(menuHistory)

        if(preset_id == -1) {

            mymodel.insert(
                mymodel.rowCount(),
                mymodel.index(mymodel.rowCount(), 0),
                {
                    "expanded": false,
                    "name": menuHistory,
                    "queries": [
                        {
                            "enabled": (pstep !== undefined && pstep !== ""),
                            "livelink": false,
                            "dynamic": true,
                            "term": "Pipeline Step",
                            "value": pstep ? pstep :""
                        },
                        {
                            "enabled": true,
                            "term": is_shot ? "Shot" : "Sequence",
                            "dynamic": true,
                            "value": shot_seq
                        },
                        {
                            "enabled": (twigtype !== undefined && twigtype !== ""),
                            "livelink": !(twigtype !== undefined && twigtype !== ""),
                            "dynamic": true,
                            "term": "Twig Type",
                            "value": twigtype ? twigtype :""
                        },
                        {
                            "enabled": true,
                            "livelink": false,
                            "dynamic": true,
                            "term": "Twig Name",
                            "value": "^"+twigname+"$"
                        },
                        {
                            "enabled": false,
                            "term": "Latest Version",
                            "value": "True"
                        },
                        {
                            "enabled": false,
                            "term": "Sent To Client",
                            "value": "True"
                        },
                        {
                            "enabled": true,
                            "term": "Flag Media",
                            "value": "Orange"
                        }
                    ]
                }
            )
            preset_id = mymodel.rowCount()-1
        } else {
            // update shot,,,
            // will break if user fiddles...
            let pindex = mymodel.index(preset_id, 0)
            mymodel.set(0, pstep ? pstep :"", "argRole", pindex);

            let term = mymodel.get(1, pindex, "termRole")
            if(term == "Shot" && !is_shot) {
                mymodel.set(1, "Sequence", "termRole", pindex);
            } else if(term == "Sequence" && is_shot) {
                mymodel.set(1, "Shot", "termRole", pindex);
            }

            mymodel.set(1, shot_seq, "argRole", pindex);

            mymodel.set(2, twigtype ? twigtype :"", "argRole", pindex);
            mymodel.set(3, "^"+twigname+"$", "argRole", pindex);
        }

        if(mymodel.activePreset == preset_id) {
            executeQuery()
        } else {
            mymodel.activePreset = preset_id
        }

        treeTab.selectItem(
            sequenceTreeModel.search_recursive(
                shot_seq,
                "nameRole"
            )
        )
    }

    function createOrUpdate(term_type, term_value, mode) {
        let preset_id = -1;

        if(mode == "Versions" ) {
            preset_id = searchTreePresetsViewModel.search(menuLatest)
        } else if(mode == "Notes" ) {
            preset_id = searchTreePresetsViewModel.search("All")
        }

        if(preset_id != -1) {

            // update preset..
            setShotSequenceTermPreset(term_type, term_value, preset_id)
            // if(searchTreePresetsViewModel.activePreset == preset_id) {
            //     searchTreePresetsViewModel.activePreset = -1
            // }

        } else {
            // create missing preset..
            if(mode == "Versions" ) {
                searchTreePresetsViewModel.insert(
                    searchTreePresetsViewModel.rowCount(),
                    searchTreePresetsViewModel.index(searchTreePresetsViewModel.rowCount(), 0),
                    {
                        "expanded": false,
                        "name": menuLatest,
                        "queries": [
                            {
                                "enabled": true,
                                "term": term_type == "Shot" ? "Shot" : "Sequence",
                                "dynamic": true,
                                "value": term_value
                            },
                            {
                                "enabled": true,
                                "term": "Latest Version",
                                "value": "True"
                            },
                            {
                              "enabled": true,
                              "livelink": false,
                              "term": "Twig Type",
                              "value": "scan"
                            },
                            {
                              "enabled": true,
                              "livelink": false,
                              "term": "Twig Type",
                              "value": "render/element"
                            },
                            {
                              "enabled": true,
                              "livelink": false,
                              "term": "Twig Type",
                              "value": "render/out"
                            },
                            {
                              "enabled": true,
                              "livelink": false,
                              "term": "Twig Type",
                              "value": "render/playblast"
                            },
                            {
                              "enabled": true,
                              "livelink": false,
                              "term": "Twig Type",
                              "value": "render/playblast/working"
                            },
                            {
                                "enabled": true,
                                "term": "Flag Media",
                                "value": "Orange"
                            }
                        ]
                    }
                )
            } else if(mode == "Notes") {
                searchTreePresetsViewModel.insert(
                    searchTreePresetsViewModel.rowCount(),
                    searchTreePresetsViewModel.index(searchTreePresetsViewModel.rowCount(), 0),
                    {
                        "expanded": false,
                        "name": "All",
                        "queries": [
                            {
                                "enabled": true,
                                "term": term_type == "Shot" ? "Shot" : "Sequence",
                                "dynamic": true,
                                "value": term_value
                            }
                        ]
                    }
                )
            }
            preset_id = searchTreePresetsViewModel.rowCount()-1
        }

        searchTreePresetsViewModel.activePreset = preset_id
        treeTab.selectItem(
            sequenceTreeModel.search_recursive(
                term_value,
                "nameRole"
            )
        )
    }

    function setShotSequenceTermPreset(term_type, term_value, row) {
        let result = false

        for(let i =0; i< searchTreePresetsViewModel.rowCount(); i++) {
            let index = searchTreePresetsViewModel.index(i, 0)

            if(index.valid) {
                let term = searchTreePresetsViewModel.get(0, index, "termRole")
                let live = searchTreePresetsViewModel.get(0, index, "liveLinkRole")

                if(["Shot","Sequence"].includes(term)) {
                    if(live == undefined)
                        live = false

                    if(term != term_type ) { //&& !live
                        searchTreePresetsViewModel.set(0, term_type, "termRole", index);
                        searchTreePresetsViewModel.set(0, term_value, "argRole", index);
                        if(i == row) {
                            searchTreePresetsViewModel.set(0, false, "enabledRole", index);
                            searchTreePresetsViewModel.set(0, true, "enabledRole", index);
                        }
                    } else {
                        searchTreePresetsViewModel.set(0, term_value, "argRole", index);
                    }

                    if(i == row)
                        result = true
                } else {
                    //  could be history..
                    if(term == "Pipeline Step") {
                        // test for matching shot..
                        let value = searchTreePresetsViewModel.get(1, index, "argRole")
                        if(value == term_value &&  i == row)
                            result = true
                    }
                }
            } else {
                console.log("INVALID INDEX", searchTreePresetsViewModel)
            }
        }

        return result
    }

    function selectSearchedIndex(clickedIndex){

        searchTextField.text=""
        if(currentCategory == "Notes" ){
            let mymodel = notePresetsModel
            mymodel.clearLoaded()
            mymodel.insert(
                mymodel.rowCount(),
                mymodel.index(mymodel.rowCount(),0),
                {
                    "expanded": false,
                    "name": shotSearchFilterModel.get(clickedIndex, "nameRole") + " Notes",
                    "queries": [
                        {
                            "enabled": true,
                            "term": "Shot",
                            "value": shotSearchFilterModel.get(clickedIndex, "nameRole")
                        }
                    ]
                }
            )
            mymodel.activePreset = mymodel.rowCount()-1
        }
        else if(currentCategory == "Versions" ) {
            let mymodel = shotPresetsModel
            mymodel.clearLoaded()
            mymodel.insert(
                mymodel.rowCount(),
                mymodel.index(mymodel.rowCount(),0),
                {
                  "expanded": false,
                  "name": "Shot Search - " + shotSearchFilterModel.get(clickedIndex, "nameRole"),

                    "queries": [
                        {
                            "enabled": true,
                            "term": "Shot",
                            "value": shotSearchFilterModel.get(clickedIndex, "nameRole")
                        },
                        {
                            "enabled": true,
                            "term": "Latest Version",
                            "value": "True"
                        },
                        {
                          "enabled": true,
                          "livelink": false,
                          "term": "Twig Type",
                          "value": "scan"
                        },
                        {
                          "enabled": true,
                          "livelink": false,
                          "term": "Twig Type",
                          "value": "render/element"
                        },
                        {
                          "enabled": true,
                          "livelink": false,
                          "term": "Twig Type",
                          "value": "render/out"
                        },
                        {
                          "enabled": true,
                          "livelink": false,
                          "term": "Twig Type",
                          "value": "render/playblast"
                        },
                        {
                          "enabled": true,
                          "livelink": false,
                          "term": "Twig Type",
                          "value": "render/playblast/working"
                        }
                    ]
                }
            )
            mymodel.activePreset = mymodel.rowCount()-1
        } else if(currentCategory == "Versions Tree" || currentCategory == "Notes Tree") {
            // console.log(nameRole, typeRole, idRole)
            treeTab.selectItem(
                sequenceTreeModel.search_recursive(
                    shotSearchFilterModel.get(clickedIndex, "idRole"),
                    "idRole"
                )
            )
        }

        searchTextField.focus = false //closes list
    }

    ColumnLayout { id: leftView
        anchors.fill: parent
        anchors.topMargin: framePadding
        anchors.bottomMargin: framePadding
        anchors.leftMargin: framePadding
        anchors.rightMargin: 0

        Rectangle{ id: projectDiv
            Layout.fillWidth: true
            Layout.minimumHeight: itemHeight + (framePadding *2)

            color: "transparent"
            border.color: frameColor
            border.width: frameWidth

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: framePadding

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: itemHeight
                    Layout.maximumHeight: itemHeight
                    spacing: itemSpacing

                    // XsButton{ id: collapseButton
                    //     Layout.preferredWidth: itemHeight
                    //     Layout.preferredHeight: itemHeight
                    //     imgSrc: "qrc:/feather_icons/chevrons-left.svg"
                    //     rotation: isCollapsed? 180 : 0
                    //     onClicked: isCollapsed = !isCollapsed
                    // }
                    XsButton{id: authenticateButton
                        text: ""
                        imgSrc: "qrc:/feather_icons/key.svg"
                        Layout.preferredWidth: itemHeight
                        Layout.preferredHeight: itemHeight
                        onClicked: authenticateFunc()

                        // ToolTip.text: "Authenticate"
                        // ToolTip.visible: hovered
                    }

                    XsComboBox{ id: projComboBox
                        visible: !isCollapsed
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        textRole: "display"
                        model: projectModel
                        currentIndex: projectCurrentIndex
                        editable: true
                        font.bold: true

                        onAccepted: focus = false
                        onFocusChanged: if(!focus) accepted()
                        onCurrentIndexChanged: {
                            let pid = projectModel.get(currentIndex, "idRole")
                            project_id_preference.value = pid

                            if(sequenceModelFunc) {
                                sequenceModel = sequenceModelFunc(pid)
                            }
                            if(sequenceTreeModelFunc) {
                                sequenceTreeModel = sequenceTreeModelFunc(pid)
                            }
                            if(shotModelFunc) {
                                shotModel = shotModelFunc(pid)
                            }
                            if(customEntity24ModelFunc) {
                                customEntity24Model = customEntity24ModelFunc(pid)
                            }
                            if(shotSearchFilterModelFunc) {
                                shotSearchFilterModel = shotSearchFilterModelFunc(pid)
                            }
                            if(playlistModelFunc) {
                                playlistModel = playlistModelFunc(pid)
                            }

                            // clear loaded and selected presets.
                            projectChanged(pid)

                            if(liveLinkProjectChange) {
                                let tmp = searchPresetsView.currentIndex
                                searchPresetsView.currentIndex = -1
                                searchPresetsView.currentIndex = tmp
                                liveLinkProjectChange = false
                            } else {
                                searchPresetsView.currentIndex = -1
                                presetSelectionModel.clear()
                                presetTreeSelectionModel.clear()
                            }
                        }
                    }

                    XsButton{id: filtButton
                        visible: !isCollapsed
                        text: ""
                        imgSrc: "qrc:/feather_icons/filter.svg"
                        width: height
                        Layout.preferredHeight: itemHeight
                        Layout.preferredWidth: itemHeight
                        onClicked: {
                            onClicked: popup.open()
                        }
                        subtleActive: true
                        isActive: filterViewModel.hasActiveFilter
                    }

                    Popup {
                        id: popup
                        x: filtButton.x + filtButton.width + itemSpacing
                        y: filtButton.y
                        width: 400
                        height: 400
                        modal: true
                        focus: true
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                        margins: 0
                        padding: 0


                        // we need to somehow update these...
                        Connections {
                            target: preFilterListView.queryModel
                            function onJsonChanged() {
                                preFilterListView.queryRootIndex = filterViewModel.index(0,0)
                            }
                        }

                        onAboutToShow: {
                            preFilterListView.queryModel = filterViewModel
                            preFilterListView.queryRootIndex = filterViewModel.index(0,0)
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "#222"
                            border.color: frameColor
                            border.width: frameWidth

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: framePadding

                                QueryListView {
                                    id: preFilterListView
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true

                                    onSnapshot: snapshotGlobals()
                                    // queryModel: filterViewModel.length ? filterViewModel : null
                                    // queryRootIndex: filterViewModel.index(0,0)
                                }

                                XsButton{
                                    text: "Close"
                                    Layout.preferredHeight: itemHeight*1
                                    Layout.alignment: Qt.AlignRight
                                    onClicked: popup.close()
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle{ id: categoryDiv
            property real divHeight: 40
            Layout.fillWidth: true
            Layout.minimumHeight: divHeight + (framePadding *2) //itemHeight + (framePadding *2)
            color: "transparent"
            border.color: frameColor
            border.width: frameWidth

            RowLayout{
                spacing: itemSpacing
                x: framePadding
                y: framePadding
                width: parent.width - framePadding*2
                height: parent.height - framePadding*2
                clip: true

                Repeater { id: categoriesRep
                    width: parent.width
                    height: parent.height
                    model: ["Playlists", "Versions", "Reference", "Notes", "Menu Setup"]

                    XsButton{ id: categoryButton
                        Layout.fillHeight: true
                        text: ""
                        isActive: currentCategoryClass === modelData
                        hoverEnabled: enabled

                        ToolTip {
                            parent: categoryButton
                            visible: categoryButton.hovered && modelData == "Menu Setup" //tText.truncated
                            text: modelData
                        }

                        Text{
                            id: tText
                            text: modelData == "Menu Setup"? "" : modelData

                            font.pixelSize: fontSize
                            font.family: fontFamily
                            color: enabled? currentCategoryClass === modelData || categoryButton.down || categoryButton.hovered || parent.isActive? textColorActive: textColorNormal : Qt.darker(textColorNormal,1.5)
                            horizontalAlignment: Text.AlignHCenter

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: framePadding/2
                        }
                        Image {
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: (model.index==4)? (categoryButton.height-height)/2 : framePadding/2
                            width: 20
                            height: width
                            source: //["Playlists", "Versions", "Reference", "Notes", "Menu Setup"]
                                if(model.index==0) "qrc:///feather_icons/list.svg"
                                else if(model.index==1) "qrc:///feather_icons/film.svg"
                                else if(model.index==2) "qrc:///feather_icons/image.svg"
                                else if(model.index==3) "qrc:///feather_icons/message-square.svg"
                                else if(model.index==4) "qrc:///feather_icons/menu.svg"
                            anchors.horizontalCenter: parent.horizontalCenter
                            layer {
                                enabled: true
                                effect:
                                ColorOverlay {
                                    color: enabled? currentCategoryClass === modelData || categoryButton.down || categoryButton.hovered || parent.isActive? textColorActive: textColorNormal : Qt.darker(textColorNormal,1.5)
                                }
                            }
                        }

                        onClicked: {
                            shotgunBrowser.categorySwitchedOnClick= true
                            if(modelData in categoryLastMode) {
                                currentCategory = categoryLastMode[modelData]
                            } else {
                                currentCategory = modelData
                                categoryLastMode[currentCategory] = currentCategory
                            }
                            // context_pref2.value = currentCategory
                            rightDiv.selectionModel.resetSelection(true)
                        }

                        Component.onCompleted: {
                            if(modelData == "Menu Setup"){
                                Layout.fillWidth= false
                                Layout.preferredWidth= 28 //58

                            } else{
                                Layout.fillWidth= true
                            }
                        }

                    }
                }
            }
        }

        Rectangle{ id: buttonsDiv
            z: 1
            // enabled: !isCollapsed
            color: "transparent"
            radius: frameRadius
            border.color: frameColor
            border.width: frameWidth
            visible: ["Versions"].includes(currentCategoryClass) || ( isTreeVisible && ["Versions","Notes"].includes(currentCategoryClass) )

            property bool isTreeVisible: true

            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight + framePadding*2

            RowLayout{
                x: framePadding
                spacing: itemSpacing
                anchors.verticalCenter: parent.verticalCenter
                width: buttonsDiv.isTreeVisible? parent.width -framePadding*2 : parent.width -framePadding
                clip: true

                XsTabBar{id: tabBar
                    Layout.fillWidth: buttonsDiv.isTreeVisible? true: false
                    contentHeight: buttonsDiv.isTreeVisible? itemHeight : 0

                    XsTabButton {
                        text: qsTr("Find")
                        isActive: tabBar.currentIndex == 0
                        width: tabBar.width/2-frameWidth
                    }
                    XsTabButton {
                        text: qsTr("Tree")
                        isActive: tabBar.currentIndex == 1
                        width: tabBar.width/2-frameWidth
                    }
                }

                Rectangle{ id: searchField
                    Layout.preferredWidth: buttonsDiv.isTreeVisible? parent.width/2 : parent.width-framePadding
                    height: itemHeight
                    color: "transparent"
                    border.color: frameColor
                    border.width: frameWidth

                    XsTextField {
                        id: searchTextField
                        // enabled: !["Notes"].includes(currentCategoryClass)
                        width: parent.width
                        height: itemHeight
                        font.pixelSize: fontSize*1.2
                        placeholderText: "Search"
                        onAccepted: {
                            focus = false
                            if(shotSearchFilterModel.count>0) {
                                if(searchShotListPopup.currentIndex==-1) searchShotListPopup.currentIndex=0
                                searchShotListPopup.focus = true
                            }
                        }
                        onTextEdited: shotSearchFilterModel.setFilterWildcard(text)

                        onFocusChanged: {
                            if(focus) searchShotListPopup.currentIndex=-1
                        }
                        Keys.onPressed: (event)=> {
                            if (event.key == Qt.Key_Down) {
                                if(shotSearchFilterModel.count>0) {
                                    if(searchShotListPopup.currentIndex===-1) searchShotListPopup.currentIndex=0
                                    else if(searchShotListPopup.currentIndex!==-1) searchShotListPopup.currentIndex+=1
                                    searchShotListPopup.focus = true
                                }
                            }
                            else if (event.key == Qt.Key_Up) {
                                if(shotSearchFilterModel.count>0) {
                                    if(searchShotListPopup.currentIndex!==-1) {
                                        searchShotListPopup.currentIndex-=1
                                        searchShotListPopup.focus = true
                                    }
                                }
                            }
                        }
                    }
                    XsButton{
                        id: clearButton
                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        visible: searchTextField.length !== 0
                        width: height
                        height: itemHeight - framePadding
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: framePadding/2
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        onClicked: {
                            searchTextField.clear()
                            searchTextField.textEdited()
                        }
                    }

                }
            }

            ListView{ id: searchShotListPopup
                z: 10
                property real searchItemHeight: itemHeight/1.3
                property int searchModelCount: shotSearchFilterModel ? shotSearchFilterModel.count : 0
                model: shotSearchFilterModel
                Rectangle{ anchors.fill: parent; color: "transparent";
                border.color: Qt.lighter(frameColor, 1.3); border.width: frameWidth }

                visible: searchTextField.text.length !== 0
                clip: true
                orientation: ListView.Vertical
                snapMode: ListView.SnapToItem
                currentIndex: -1

                x: searchField.x + framePadding
                y: searchField.y + searchField.height + framePadding
                width: searchField.width
                height: searchModelCount>=12? searchItemHeight*12 : searchItemHeight*searchModelCount
                ScrollBar.vertical: XsScrollBar { id: searchScrollBar; padding: 2}//; thumbWidth: 8}
                onContentHeightChanged: {
                    if(height < contentHeight) searchScrollBar.policy = ScrollBar.AlwaysOn
                    else {
                        searchScrollBar.policy = ScrollBar.AsNeeded
                    }
                }

                Keys.onUpPressed: {
                    if(currentIndex!=-1) currentIndex-=1

                    if(currentIndex==-1) searchTextField.focus=true
                }
                Keys.onDownPressed: {
                    if(currentIndex!=model.count-1) currentIndex+=1
                }
                Keys.onPressed: (event)=> {
                    if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return) {
                        selectSearchedIndex(currentIndex)
                    }
                }

                delegate: Rectangle{
                    property bool isHovered: mouseArea.containsMouse
                    width: searchShotListPopup.width
                    height: searchShotListPopup.searchItemHeight
                    color: searchShotListPopup.currentIndex==index? Qt.darker(itemColorActive, 2.75) : Qt.darker(palette.base, 1.5)

                    Text{
                        text: nameRole
                        font.pixelSize: fontSize
                        font.family: XsStyle.fontFamily
                        font.weight: Font.Normal
                        color: searchShotListPopup.currentIndex==index? textColorActive: textColorNormal
                        elide: Text.ElideRight
                        width: parent.width
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        ToolTip.text: nameRole
                        ToolTip.visible: mouseArea.containsMouse && truncated
                    }

                    MouseArea{id: mouseArea; anchors.fill: parent; hoverEnabled: true

                        onClicked: selectSearchedIndex(index)

                        onEntered: searchShotListPopup.currentIndex=index
                    }
                }
            }


        }


        StackLayout {
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex

            onCurrentIndexChanged: {
                if(currentIndex == 1) {
                    currentCategory = currentCategoryClass + " Tree"
                } else {
                    currentCategory = currentCategoryClass
                }
                categoryLastMode[currentCategoryClass] = currentCategory
                // context_pref2.value = currentCategory
            }

            LeftPresetView{id: presetsDiv
                isActiveView: !treeMode
                backendModel: searchPresetsViewModel
                onBackendModelChanged: {
                    if(searchPresetsViewModel) {
                        searchPresetsView.currentIndex = searchPresetsViewModel.activePreset
                        // console.log("onBackendModelChanged", searchPresetsViewModel, searchPresetsViewModel.activePreset)
                    }
                }
            }

            XsSplitView { id: leftAndRightDivs
                orientation: Qt.Vertical

                Connections {
                    target: searchTreePresetsViewModel
                    function onActiveShotChanged() {
                        if(treeMode) {
                            let index = sequenceTreeModel.search_recursive(searchTreePresetsViewModel.activeShot, "nameRole")
                            if(index.valid)
                                treeTab.selectItem(index)
                        }
                    }
                    // function onActiveSeqChanged() {
                    //     if(treeMode) {
                    //         let index = sequenceTreeModel.search_recursive(searchTreePresetsViewModel.activeSeq, "nameRole")
                    //         treeTab.selectItem(index)
                    //     }
                    // }
                }

                LeftTreeView{id: treeTab
                    SplitView.minimumWidth: parent.width
                    SplitView.minimumHeight: parent.height/8
                    SplitView.fillHeight: true

                    onClicked: {
                        if(searchTreePresetsViewModel.activePreset == -1) {
                            createOrUpdate(type, name, currentCategoryClass)
                            setShotSequenceTermPreset(type, name, -1)
                        } else {
                            if(!setShotSequenceTermPreset(type, name, searchTreePresetsViewModel.activePreset)) {
                                createOrUpdate(type, name, currentCategoryClass)
                            }
                        }
                    }

                    onDoubleClicked: {
                        createOrUpdate(type, name, currentCategoryClass)
                        setShotSequenceTermPreset(type, name, -1)
                    }
                }
                LeftPresetView{id: presetsMiniDiv
                    SplitView.minimumWidth: parent.width
                    SplitView.minimumHeight: (itemHeight * 2)  + (framePadding*2) + itemSpacing
                    SplitView.preferredHeight: (itemHeight + framePadding*2) + (presetTitleHeight+itemSpacing)*2.5
                    isActiveView: treeMode
                    backendModel: searchTreePresetsViewModel
                    onBackendModelChanged: {
                        if(searchTreePresetsViewModel) {
                            presetsMiniDiv.searchPresetsView.currentIndex = searchTreePresetsViewModel.activePreset
                        }
                    }
                }
            }
        }
    }
}
