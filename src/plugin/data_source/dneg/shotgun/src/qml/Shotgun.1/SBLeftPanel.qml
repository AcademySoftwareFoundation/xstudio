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

    property string currentCategory: "Playlists"

    property bool liveLinkProjectChange: false
    property var authenticateFunc: null

    property var authorModel: null
    property var siteModel: null
    property var departmentModel: null
    property var noteTypeModel: null
    property var playlistTypeModel: null
    property var productionStatusModel: null
    property var pipelineStatusModel: null

    property var boolModel: null
    property var resultLimitModel: null
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

    property var shotSearchFilterModel: null
    property var shotSearchFilterModelFunc: null

    property var playlistModel: null
    property var playlistModelFunc: null

    property var filterViewModel: null

    property alias presetSelectionModel: presetsDiv.presetSelectionModel
    property var searchPresetsViewModel: null
    property var searchQueryViewModel: null

    property var shotPresetsModel: null
    property var playlistPresetsModel: null
    property var editPresetsModel: null
    property var referencePresetsModel: null
    property var notePresetsModel: null
    property var mediaActionPresetsModel: null

    property var shotFilterModel: null
    property var playlistFilterModel: null
    property var editFilterModel: null
    property var referenceFilterModel: null
    property var noteFilterModel: null
    property var mediaActionFilterModel: null

    property var executeQueryFunc: null
    property var mergeQueriesFunc: null

    property alias presetsModel: presetsDiv.presetsModel
    property alias searchPresetsView: presetsDiv.searchPresetsView
    property alias presetsDiv: presetsDiv

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
"On Disk",
"Order By",
"Pipeline Status",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Production Status",
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
"On Disk",
"Order By",
"Pipeline Status",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Production Status",
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
"Result Limit",
"Shot Status",
"Site",
"Tag"
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
"Note Type",
"Order By",
"Pipeline Step",
"Playlist",
"Preferred Audio",
"Preferred Visual",
"Recipient",
"Result Limit",
"Shot Status",
"Shot",
"Tag",
"Twig Name",
"Twig Type",
"Version Name"
    ]

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
        if(!launch_query.running)
            launch_query.start()
    }

    XsErrorMessage {
        id: error_dialog
        title: "Shot Browser Error"
        width: 200
        height: 100
    }

    function executeQueryReal() {
        if(currentCategory) {
            let query = mergeQueriesFunc(presetsModel.get(searchPresetsView.currentIndex, "jsonRole"), filterViewModel.get(0, "jsonRole"))

            let preset_index = searchPresetsView.currentIndex
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
        }
    }

    function clearResults() {
        rightDiv.searchResultsViewModel.sourceModel.clear()
        // rightDiv.searchPresetsView.clearLoaded()
        rightDiv.clearFilter()
        presetSelectionModel.clear()
        searchPresetsView.currentIndex = -1
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
                            project_id_pref.properties.value = pid

                            if(sequenceModelFunc) {
                                sequenceModel = sequenceModelFunc(pid)
                            }
                            if(sequenceTreeModelFunc) {
                                sequenceTreeModel = sequenceTreeModelFunc(pid)
                            }
                            if(shotModelFunc) {
                                shotModel = shotModelFunc(pid)
                            }
                            if(shotSearchFilterModelFunc) {
                                shotSearchFilterModel = shotSearchFilterModelFunc(pid)
                            }
                            if(playlistModelFunc) {
                                playlistModel = playlistModelFunc(pid)
                            }

                            rightDiv.clearFilter()
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
                                    queryModel: filterViewModel.length != 0  ? filterViewModel : null
                                    queryRootIndex: filterViewModel.length != 0 ?  filterViewModel.index(0, 0) : null
                                    isLoaded: searchPresetsView.currentIndex != -1
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
                    model: ["Playlists", "Shots", "Reference", "Notes", "Menu Setup"]

                    XsButton{ id: categoryButton
                        Layout.fillHeight: true
                        text: ""
                        isActive: currentCategory === modelData
                        enabled: text !== "Edits"
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
                            color: enabled? currentCategory === modelData || categoryButton.down || categoryButton.hovered || parent.isActive? textColorActive: textColorNormal : Qt.darker(textColorNormal,1.5)
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
                            source: //["Playlists", "Shots", "Reference", "Notes", "Menu Setup"]
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
                                    color: enabled? currentCategory === modelData || categoryButton.down || categoryButton.hovered || parent.isActive? textColorActive: textColorNormal : Qt.darker(textColorNormal,1.5)
                                }
                            }
                        }

                        onClicked: {
                            shotgunBrowser.categorySwitchedOnClick= true
                            currentCategory= modelData
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
            visible: ["Shots"].includes(currentCategory) || ( isTreeVisible && ["Shots","Notes"].includes(currentCategory) )

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
                        text: qsTr("Presets")
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
                        enabled: !(["Notes"].includes(currentCategory) && placeholderText=="Shot Search")
                        width: parent.width
                        height: itemHeight
                        font.pixelSize: fontSize*1.2
                        placeholderText: tabBar.currentIndex==0?currentCategory=="Reference"?"Reference Search" : "Shot Search" : "Tree Search"
                        onAccepted: focus = false
                        onTextEdited: shotSearchFilterModel.setFilterWildcard(text)
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

                visible: searchTextField.text.length !== 0 && searchTextField.activeFocus
                clip: true
                orientation: ListView.Vertical
                snapMode: ListView.SnapToItem
                currentIndex: -1

                x: searchField.x + framePadding
                y: searchField.y + searchField.height + framePadding
                // anchors.horizontalCenter: searchField.horizontalCenter
                // anchors.top: searchField.bottom
                // anchors.topMargin: -framePadding
                width: searchField.width
                height: searchModelCount>=12? searchItemHeight*12 : searchItemHeight*searchModelCount
                ScrollBar.vertical: XsScrollBar { id: searchScrollBar; padding: 2}//; thumbWidth: 8}
                onContentHeightChanged: {
                    if(height < contentHeight) searchScrollBar.policy = ScrollBar.AlwaysOn
                    else {
                        searchScrollBar.policy = ScrollBar.AsNeeded
                    }
                }

                delegate: Rectangle{
                    property bool isHovered: mouseArea.containsMouse
                    width: searchShotListPopup.width
                    height: searchShotListPopup.searchItemHeight
                    color: isHovered? Qt.darker(itemColorActive, 2.75) : Qt.darker(palette.base, 1.5)

                    Text{
                        text: nameRole
                        font.pixelSize: fontSize
                        font.family: XsStyle.fontFamily
                        font.weight: Font.Normal
                        color: isHovered? textColorActive: textColorNormal
                        elide: Text.ElideRight
                        width: parent.width
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        ToolTip.text: nameRole
                        ToolTip.visible: mouseArea.containsMouse && truncated
                    }

                    MouseArea{id: mouseArea; anchors.fill: parent; hoverEnabled: true

                        onClicked: {
                            searchTextField.text=""
                            if(currentCategory == "Notes" ){
                                notePresetsModel.clearLoaded()
                                notePresetsModel.insert(
                                    notePresetsModel.rowCount(),
                                    notePresetsModel.index(notePresetsModel.rowCount(),0),
                                    {
                                        "expanded": false,
                                        "loaded": true,
                                        "name": nameRole + " Notes",
                                        "queries": [
                                            {
                                                "enabled": true,
                                                "term": "Shot",
                                                "value": nameRole
                                            }
                                        ]
                                    }
                                )
                                rightDiv.forceSelectPreset(notePresetsModel.rowCount()-1)
                            }
                            else{
                                shotPresetsModel.clearLoaded()
                                shotPresetsModel.insert(
                                    shotPresetsModel.rowCount(),
                                    shotPresetsModel.index(shotPresetsModel.rowCount(),0),
                                    {
                                      "expanded": false,
                                      "loaded": true,
                                      "name": "Shot Search - " + nameRole,

                                        "queries": [
                                            {
                                                "enabled": true,
                                                "term": "Shot",
                                                "value": nameRole
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
                                rightDiv.forceSelectPreset(shotPresetsModel.rowCount()-1)
                            }

                            searchTextField.focus = false //closes list
                        }
                    }
                }
            }

        }


        StackLayout {
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex
            property string category: currentCategory
            onCategoryChanged:{
                if( !["Shots","Notes"].includes(currentCategory) ) tabBar.currentIndex=0
            }

            LeftPresetView{id: presetsDiv
                backendModel: searchPresetsViewModel
            }

            XsSplitView { id: leftAndRightDivs
                orientation: Qt.Vertical

                LeftTreeView{id: treeTab
                    SplitView.minimumWidth: parent.width
                    SplitView.minimumHeight: parent.height/2
                    SplitView.fillHeight: true
                    // SplitView.preferredHeight:  SplitView.minimumHeight

                    // backendModel: sequenceTreeModel
                }
                LeftPresetView{id: presetsMiniDiv
                    SplitView.minimumWidth: parent.width
                    SplitView.minimumHeight: (itemHeight + framePadding*2) + (presetTitleHeight + presetTitleHeight/2 + itemSpacing)
                    SplitView.preferredHeight: (itemHeight + framePadding*2) + (presetTitleHeight+itemSpacing)*2.5

                    backendModel: searchPresetsViewModel
                }
            }

        }
    }

}
