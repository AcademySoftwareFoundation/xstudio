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
import Qt.labs.qmlmodels 1.0
import Qt.labs.platform 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1

Rectangle{ id: rightDiv
    property var searchResultsViewModel
    onSearchResultsViewModelChanged:{
        searchResultsViewModel.setFilterWildcard(filterTextField.text)
    }

    property var currentPresetIndex: -1

    property var loadPlaylists: dummyFunction
    property var addShotsToNewPlaylist: dummyFunction
    property var addShotsToPlaylist: dummyFunction
    property var addAndCompareShotsToPlaylist: dummyFunction

    property alias selectionModel: selectionModel

    // dynamic filters
    property alias pipelineStepFilterIndex: filterStepBox.currentIndex
    property alias onDiskFilterIndex: filterOnDiskBox.currentIndex

    color: "transparent"
    signal createPreset(query: string, value: string)
    signal forceSelectPreset(index: int)

    signal showLatestVersion(type: string, name: string, id: int, mode: string)
    signal showVersionHistory(shot_seq: string, is_shot: bool, twigname: string, pstep: string, twigtype: string)

    SplitView.minimumWidth: minimumRightSplitViewWidth //+ (minimumWidth - (minimumSplitViewWidth*2))
    SplitView.preferredWidth: minimumRightSplitViewWidth //+ (minimumWidth - (minimumSplitViewWidth*2))
    SplitView.minimumHeight: parent.height

    MouseArea{ id: rightDivMArea; anchors.fill: parent; onPressed: focus= true; onReleased: focus= false; }

    function clearFilter() {
        filterTextField.clear()
        filterTextField.textEdited()
        filterStepBox.currentIndex = -1
    }

    function popupMenuAction(actionText, index=-1)
    {
        if(actionText == menuLatest) {
            let i = index

            if(i == -1) {
                if(!selectionModel.selectedIndexes.length)
                   return
               i = selectionModel.selectedIndexes[0].row
            }

            let shot = searchResultsViewModel.get(i,"shotRole")

            showLatestVersion("Shot", shot, 0, "Versions")
        } else if(actionText == menuHistory) {
            let i = index

            if(i == -1) {
                if(!selectionModel.selectedIndexes.length)
                   return
               i = selectionModel.selectedIndexes[0].row
            }

            let shot = searchResultsViewModel.get(i,"shotRole")
            let seq = searchResultsViewModel.get(i,"sequenceRole")
            let twigname = searchResultsViewModel.get(i,"twigNameRole")
            let pstep = searchResultsViewModel.get(i,"pipelineStepRole")
            let twigtype = searchResultsViewModel.get(i,"twigTypeRole")

            showVersionHistory(shot ? shot : seq, shot ? true : false, twigname, pstep, twigtype)
        } else if(actionText == "Select All") {
            selectionModel.resetSelection()
            for(let idx=0; idx<searchResultsViewModel.count; idx++){
                selectionModel.select(searchResultsViewModel.index(idx, 0), ItemSelectionModel.Select)
                selectionModel.countSelection(true)
            }
        } else if(actionText == "Deselect All") {
            selectionModel.select(searchResultsViewModel.index(0, 0), ItemSelectionModel.Clear)
            selectionModel.resetSelection()
        } else if(actionText == "Invert Selection") {
            selectionModel.resetSelection()
            for(let idx=0; idx<searchResultsViewModel.count; idx++){
                selectionModel.select(searchResultsViewModel.index(idx, 0), ItemSelectionModel.Toggle)
                let isSelected =  selectionModel.isSelected(searchResultsViewModel.index(idx, 0))
                if(isSelected) selectionModel.countSelection(true)
            }
        } else if(actionText == "Descending") {
            searchResultsViewModel.sortAscending = false
        } else if(actionText == "Ascending") {
            searchResultsViewModel.sortAscending = true
        } else if(actionText == "Version Name") {
            searchResultsViewModel.sortRoleName = "nameRole"
        } else if(actionText == "Shot Name") {
            searchResultsViewModel.sortRoleName = "shotRole"
        } else if(actionText == "Creation Date") {
            searchResultsViewModel.sortRoleName = "createdDateRole"
        } else if(actionText == "Reveal In ShotGrid") {
            // get selection..
            let i = selectionModel.selectedIndexes[0]
            helpers.openURL(searchResultsViewModel.get(i.row,"URLRole"))
        } else if(actionText == "Reveal In Ivy") {
            // get selection..
            let quuid = searchResultsViewModel.get(selectionModel.selectedIndexes[0].row,"stalkUuidRole")
            // dnenv-do $SHOW $SHOT -- ivybrowser
            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), helpers.getEnv("SHOT"), "--", "ivybrowser", helpers.QUuidToQString(quuid)])
            // helpers.startDetachedProcess("env", ["BOB_DEPLOYMENT_PATH="+helpers.getEnv("BOB_UNWORLD_PREVIOUS_DEPLOYMENT_PATH"), "bob-unworld", "ivybrowser", helpers.QUuidToQString(quuid)])
        } else if(actionText == "maketransfer") {
            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), helpers.getEnv("SHOT"), "--", "maketransfer"])
        } else if(actionText.startsWith("Transfer")) {
            let dstloc = actionText.replace("Transfer ", "")
            let show = ""
            let srcloc = ""
            let uuids = []
            for(let i=0; i < selectionModel.selectedIndexes.length; i++) {
                let dnuuid = searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"stalkUuidRole")
                if(show == "") {
                    show = searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"projectRole")
                }
                // where is it...
                if(srcloc == "") {
                    if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteLon") && dstloc != "lon")
                        srcloc = "lon"
                    else if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteMum") && dstloc != "mum")
                        srcloc = "mum"
                    else if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteVan") && dstloc != "van")
                        srcloc = "van"
                    else if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteChn") && dstloc != "chn")
                        srcloc = "chn"
                    else if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteMtl") && dstloc != "mtl")
                        srcloc = "mtl"
                    else if(searchResultsViewModel.get(selectionModel.selectedIndexes[i].row,"onSiteSyd") && dstloc != "syd")
                        srcloc = "syd"
                }
                uuids.push(dnuuid)
            }

            var fa = data_source.requestFileTransferFuture(uuids, show, srcloc, dstloc)

            Future.promise(fa).then(
                function(result) {}
            )
        } else {
            console.log(actionText, index)
        }
    }

    function dummyFunction(id_list) {}

    // requires special handling for notes..
    function applySelection(func, singleIndex=-1) {
        let result = []

        if(currentCategoryClass == "Notes") {
            // collect version ids..
            let project_id = 0

            let versions = []
            if(singleIndex == -1){
                selectionModel.selectedIndexes.forEach(
                    function (item, index) {
                        let tmp = searchResultsViewModel.get(item.row, "linkedVersionsRole")
                        if(!project_id)
                            project_id = searchResultsViewModel.get(index, "projectIdRole")
                        tmp.forEach(
                            function (item, index) {
                                versions.push(item)
                            }
                        )
                    }
                )
            } else {
                versions = searchResultsViewModel.get(singleIndex, "linkedVersionsRole")
                project_id = searchResultsViewModel.get(singleIndex, "projectIdRole")
            }

            var fa = data_source.getVersionsFuture(project_id,versions)

            Future.promise(fa).then(
                function(json_string) {
                    // turn into array of versions
                    try {
                        var data = JSON.parse(json_string)
                        data["data"].forEach(
                            function (item, index) {
                                result.push(
                                    {
                                        "id": item.id,
                                        "name": item.attributes.code,
                                        "json": item
                                    }
                                )
                            }
                        )

                        func(result)
                    } catch(err){
                        console.log(err)
                    }
                }
            )
        } else {
            if(singleIndex == -1){
                selectionModel.selectedIndexes.forEach(
                    function (item, index) {
                        result.push(
                            {
                                "id": searchResultsViewModel.get(item.row, "idRole"),
                                "name": searchResultsViewModel.get(item.row, "nameRole"),
                                "json": searchResultsViewModel.get(item.row, "jsonRole")
                            }
                        )
                    }
                )
            } else{
                result.push(
                    {
                        "id": searchResultsViewModel.get(singleIndex, "idRole"),
                        "name": searchResultsViewModel.get(singleIndex, "nameRole"),
                        "json": searchResultsViewModel.get(singleIndex, "jsonRole")
                    }
                )
            }

            func(result)
        }
    }

    // SplitView{
    Rectangle{
        id: rightView
        y: framePadding
        width: parent.width - framePadding
        height: parent.height
        color: "transparent"

        Rectangle{ id: filterSortDiv
            color: "transparent"
            radius: frameRadius
            border.color: frameColor
            border.width: frameWidth

            // anchors.top: categoryDiv.bottom
            // anchors.topMargin: framePadding

            width: parent.width
            height: itemHeight + framePadding*2

            XsButton{ id: sortButton
                text: ""
                imgSrc: if(searchResultsViewModel.sortRoleName == "shotRole" && searchResultsViewModel.sortAscending) "qrc:/icons/sort_az_down.png"
                        else if(searchResultsViewModel.sortRoleName == "shotRole") "qrc:/icons/sort_az_up.png"
                        else if(searchResultsViewModel.sortRoleName == "nameRole" && searchResultsViewModel.sortAscending) (currentCategoryClass == "Playlists")? "qrc:/icons/sort_az_down.png" : "qrc:/icons/sort_ver_down.png"
                        else if(searchResultsViewModel.sortRoleName == "nameRole") (currentCategoryClass == "Playlists")? "qrc:/icons/sort_az_up.png" : "qrc:/icons/sort_ver_up.png"
                        else if(searchResultsViewModel.sortRoleName == "versionRole" && searchResultsViewModel.sortAscending) "qrc:/icons/sort_ver_down.png"
                        else if(searchResultsViewModel.sortRoleName == "versionRole") "qrc:/icons/sort_ver_up.png"
                        else if(searchResultsViewModel.sortRoleName == "createdDateRole" && searchResultsViewModel.sortAscending) "qrc:/icons/sort_clock_down.png"
                        else if(searchResultsViewModel.sortRoleName == "createdDateRole") "qrc:/icons/sort_clock_up.png"
                width: itemHeight
                height: itemHeight
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: framePadding
                onClicked: sortMenu.open()

                // ToolTip.text: "Sort"
                // ToolTip.visible: hovered
            }
            XsMenu {
                id: sortMenu
                title: "Sort By"
                fakeDisabled: false
                width: 125
                x: sortButton.x
                y: sortButton.height + itemHeight/2

                ActionGroup{id: sortTypeGroup}
                XsMenuItem {
                    mytext: currentCategory == "Playlists"?"Playlist Name":"Version Name"
                    checkable: enabled
                    visible: true
                    checked: searchResultsViewModel.sortRoleName == "nameRole"
                    actiongroup: sortTypeGroup
                    onTriggered: {
                        searchResultsViewModel.sortRoleName = "nameRole"
                    }
                }
                XsMenuItem {
                    mytext: "Shot Name";
                    checkable: enabled
                    visible: ["Versions", "Reference", "Menu Setup"].includes(currentCategoryClass)
                    checked: searchResultsViewModel.sortRoleName == "shotRole"
                    actiongroup: sortTypeGroup
                    onTriggered: {
                        searchResultsViewModel.sortRoleName = "shotRole"
                    }
                }
                XsMenuItem {
                    mytext: "Version";
                    checkable: enabled
                    visible: ["Versions", "Reference", "Menu Setup"].includes(currentCategoryClass)
                    checked: searchResultsViewModel.sortRoleName == "versionRole"
                    actiongroup: sortTypeGroup
                    onTriggered: {
                        searchResultsViewModel.sortRoleName = "versionRole"
                    }
                }
                XsMenuItem {
                    mytext: "Creation Date"
                    checkable: enabled
                    checked: searchResultsViewModel.sortRoleName == "createdDateRole"
                    actiongroup: sortTypeGroup
                    onTriggered: {
                        searchResultsViewModel.sortRoleName = "createdDateRole"
                    }
                }
                XsMenuSeparator{ }
                // MenuItem { separator: true }

                ActionGroup{id: sortOrderGroup}
                XsMenuItem {
                    mytext: "Ascending"; checkable: true; checked: searchResultsViewModel.sortAscending
                    actiongroup: sortOrderGroup
                    onTriggered: {
                        searchResultsViewModel.sortAscending = true
                    }
                }
                XsMenuItem {
                    mytext: "Descending"; checkable: true; checked: !searchResultsViewModel.sortAscending
                    actiongroup: sortOrderGroup
                    onTriggered: {
                        searchResultsViewModel.sortAscending = false
                    }
                }

            }

            Rectangle{ anchors.fill: filterRow; color: "transparent"; border.color: Qt.lighter(frameColor, 1.1); border.width: frameWidth }
            Row{id: filterRow
                spacing: 0.1 //framePadding/2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: sortButton.right
                anchors.leftMargin: itemSpacing
                width: parent.width -itemSpacing*4 -sortButton.width -framePadding
                clip: true

                Rectangle{id: filter
                    width: parent.width - filterCount.width - filterStepBox.width - filterOnDiskBox.width
                    height: itemHeight
                    color: "transparent"
                    border.color: frameColor
                    border.width: frameWidth

                    XsTextField {
                        id: filterTextField
                        width: parent.width
                        height: itemHeight
                        font.pixelSize: fontSize*1.2
                        placeholderText: "Filter"
                        onTextEdited: searchResultsViewModel && searchResultsViewModel.setFilterWildcard(text)
                        onAccepted: focus = false
                        forcedHover: clearButton.hovered
                    }
                    XsButton{
                        id: clearButton
                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        visible: filterTextField.length !== 0
                        width: height
                        height: parent.height - framePadding
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: framePadding/2
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        onClicked: clearFilter()
                    }
                }

                XsText{id: filterCount
                    width: 90
                    height: itemHeight
                    anchors.verticalCenter: parent.verticalCenter
                    text: (searchResultsViewModel.count + " / " +  searchResultsViewModel.sourceModel.count + (searchResultsViewModel.sourceModel.truncated ? "+":""))
                }

                XsComboBox{ id: filterStepBox
                    visible: currentCategoryClass !== "Playlists"
                    width: visible? 120 + (clearStepBox.anchors.rightMargin + framePadding/2): 0
                    height: itemHeight
                    anchors.verticalCenter: parent.verticalCenter
                    editable: true
                    borderRadius: 0
                    displayText: currentIndex == -1 ? "Pipeline Step" : modelData.nameRole
                    textRole: "nameRole"
                    valueRole: "nameRole"
                    textColorNormal: currentIndex == -1 && !popupOptions.opened ? Qt.darker("dark grey", 3) :"light grey"
                    textColorActive: currentIndex == -1 && !popupOptions.opened ? Qt.darker("dark grey", 3) : palette.text
                    textColorDisabled: Qt.darker(textColorNormal, 1.6)
                    model: stepModel
                    currentIndex: -1
                    onCurrentIndexChanged:{
                        if(searchResultsViewModel) {
                            if(currentIndex == -1 || currentText == "") searchResultsViewModel.setRoleFilter("", "pipelineStepRole")
                        }
                    }
                    onCurrentTextChanged:{
                        if(searchResultsViewModel) {
                            if(currentIndex == -1 || currentText == "") searchResultsViewModel.setRoleFilter("", "pipelineStepRole")
                            else searchResultsViewModel.setRoleFilter(currentText, "pipelineStepRole")
                        }
                    }

                    Connections {
                        target: rightDiv
                        function onSearchResultsViewModelChanged() {
                            if(rightDiv.searchResultsViewModel) {
                                let i = stepModel.search(rightDiv.searchResultsViewModel.getRoleFilter("pipelineStepRole"), "nameRole", 0)
                                if(i != filterStepBox.currentIndex) {
                                    filterStepBox.currentIndex = i
                                }
                            //     if(filterStepBox.currentIndex == -1) {
                            //         searchResultsViewModel.setRoleFilter("", "pipelineStepRole")
                            //     } else {
                            //         filterStepBox.currentIndex = -1
                            //     }
                            }
                        }
                    }

                    XsButton{
                        id: clearStepBox
                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        visible: filterStepBox.currentIndex !== -1
                        width: height
                        height: parent.height - framePadding
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: framePadding*1.2+width
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        onClicked: {
                            if(filterStepBox.popupOptions.opened) filterStepBox.popupOptions.close()
                            filterStepBox.currentIndex = -1
                            filterStepBox.focus = false
                        }
                    }
                }

                XsComboBox{ id: filterOnDiskBox
                    visible: !["Playlists", "Notes"].includes(currentCategoryClass)
                    width: visible? 72 + (clearOnDiskBox.anchors.rightMargin + framePadding/2): 0
                    height: itemHeight
                    anchors.verticalCenter: parent.verticalCenter
                    editable: true
                    borderRadius: 0
                    displayText: currentIndex == -1 ? "On Disk" : modelData.nameRole
                    textRole: "nameRole"
                    valueRole: "nameRole"
                    textColorNormal: currentIndex == -1 && !popupOptions.opened ? Qt.darker("dark grey", 3) :"light grey"
                    textColorActive: currentIndex == -1 && !popupOptions.opened ? Qt.darker("dark grey", 3) : palette.text
                    textColorDisabled: Qt.darker(textColorNormal, 1.6)
                    model: onDiskModel
                    currentIndex: -1
                    onCurrentIndexChanged:{
                        if(searchResultsViewModel) {
                            if(currentIndex == -1 || currentText == ""){
                                searchResultsViewModel.setRoleFilter("", "onSiteChn")
                                searchResultsViewModel.setRoleFilter("", "onSiteLon")
                                searchResultsViewModel.setRoleFilter("", "onSiteMtl")
                                searchResultsViewModel.setRoleFilter("", "onSiteMum")
                                searchResultsViewModel.setRoleFilter("", "onSiteSyd")
                                searchResultsViewModel.setRoleFilter("", "onSiteVan")
                            }
                        }
                    }
                    onCurrentTextChanged:{
                        if(searchResultsViewModel) {

                            searchResultsViewModel.setRoleFilter("", "onSiteChn")
                            searchResultsViewModel.setRoleFilter("", "onSiteLon")
                            searchResultsViewModel.setRoleFilter("", "onSiteMtl")
                            searchResultsViewModel.setRoleFilter("", "onSiteMum")
                            searchResultsViewModel.setRoleFilter("", "onSiteSyd")
                            searchResultsViewModel.setRoleFilter("", "onSiteVan")

                            switch(currentIndex){
                                case 0:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteChn")
                                    break;
                                case 1:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteLon")
                                    break;
                                case 2:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteMtl")
                                    break;
                                case 3:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteMum")
                                    break;
                                case 4:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteSyd")
                                    break;
                                case 5:
                                    searchResultsViewModel.setRoleFilter(true, "onSiteVan")
                                    break;
                                default:
                                    searchResultsViewModel.setRoleFilter("", "onSiteChn")
                                    searchResultsViewModel.setRoleFilter("", "onSiteLon")
                                    searchResultsViewModel.setRoleFilter("", "onSiteMtl")
                                    searchResultsViewModel.setRoleFilter("", "onSiteMum")
                                    searchResultsViewModel.setRoleFilter("", "onSiteSyd")
                                    searchResultsViewModel.setRoleFilter("", "onSiteVan")
                                    break;
                            }
                        }
                    }

                    XsButton{
                        id: clearOnDiskBox
                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        visible: filterOnDiskBox.currentIndex !== -1
                        width: height
                        height: parent.height - framePadding
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: framePadding*1.2+width
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        onClicked: {
                            if(filterOnDiskBox.popupOptions.opened) filterOnDiskBox.popupOptions.close()
                            filterOnDiskBox.currentIndex = -1
                            filterOnDiskBox.focus = false
                        }
                    }
                }

            }

        }

        Rectangle{ id: searchResultsDiv
            color: "transparent"
            radius: frameRadius
            border.color: frameColor
            border.width: frameWidth

            anchors.top: filterSortDiv.bottom
            anchors.topMargin: framePadding

            width: parent.width
            height: parent.height - filterSortDiv.height - framePadding*3 //- categoryDiv.height

            ItemSelectionModel {
                id: selectionModel
                model: searchResultsViewModel
                property int selectedCount: 0
                property int prevSelectedIndex: 0

                property int displayedItemCount: searchResultsViewModel.sourceModel.count
                onDisplayedItemCountChanged: {
                    prevSelectedIndex=0
                    resetSelection()
                }

                property int shotsSelectedCount: 0
                onShotsSelectedCountChanged: selectedCount = shotsSelectedCount;
                property int shotTreeSelectedCount: 0
                onShotTreeSelectedCountChanged: selectedCount = shotTreeSelectedCount;
                property int referenceSelectedCount: 0
                onReferenceSelectedCountChanged: selectedCount = referenceSelectedCount;
                property int playlistsSelectedCount: 0
                onPlaylistsSelectedCountChanged: selectedCount = playlistsSelectedCount;
                property int notesSelectedCount: 0
                onNotesSelectedCountChanged: selectedCount = notesSelectedCount;
                property int noteTreeSelectedCount: 0
                onNoteTreeSelectedCountChanged: selectedCount = noteTreeSelectedCount;
                property int mediaActionSelectedCount: 0
                onMediaActionSelectedCountChanged: selectedCount = mediaActionSelectedCount;


                function resetSelection(categoryChanged = false) {
                    if(categoryChanged) {
                        selectedCount = 0
                        prevSelectedIndex=0
                    }

                    switch(currentCategory){
                        case "Versions":  shotsSelectedCount=0; break;
                        case "Versions Tree":  shotTreeSelectedCount=0; break;
                        case "Reference":  referenceSelectedCount=0; break;
                        case "Playlists":  playlistsSelectedCount=0; break;
                        case "Notes":  notesSelectedCount=0; break;
                        case "Notes Tree":  noteTreeSelectedCount=0; break;
                        case "Menu Setup":  mediaActionSelectedCount=0; break;
                    }
                }

                function countSelection(isSelected) {
                    if(isSelected) {
                        switch(currentCategory){
                            case "Versions":  shotsSelectedCount++; break;
                            case "Versions Tree":  shotTreeSelectedCount++; break;
                            case "Reference":  referenceSelectedCount++; break;
                            case "Playlists":  playlistsSelectedCount++; break;
                            case "Notes":  notesSelectedCount++; break;
                            case "Notes Tree":  noteTreeSelectedCount++; break;
                            case "Menu Setup":  mediaActionSelectedCount++; break;
                        }
                    }
                    else {
                        switch(currentCategory){
                            case "Versions":  shotsSelectedCount--; break;
                            case "Versions Tree":  shotTreeSelectedCount--; break;
                            case "Reference":  referenceSelectedCount--; break;
                            case "Playlists":  playlistsSelectedCount--; break;
                            case "Notes":  notesSelectedCount--; break;
                            case "Notes Tree":  noteTreeSelectedCount--; break;
                            case "Menu Setup":  mediaActionSelectedCount--; break;
                        }
                    }
                }
            }

            function itemClicked(mouse, index, isSelected){

                if (mouse.button == Qt.RightButton)
                {
                    rightClickMenu.popup()
                }
                else if (mouse.modifiers & Qt.ControlModifier) {
                    selectionModel.select(searchResultsViewModel.index(index, 0), ItemSelectionModel.Toggle)
                    selectionModel.countSelection(isSelected)
                    selectionModel.prevSelectedIndex = index
                }
                else if (mouse.modifiers & Qt.ShiftModifier) {

                    selectionModel.select(searchResultsViewModel.index(0, 0), ItemSelectionModel.Clear)
                    selectionModel.resetSelection()

                    if(index<=selectionModel.prevSelectedIndex){
                        for(let idx=index; idx<=selectionModel.prevSelectedIndex; idx++){
                            selectionModel.select(searchResultsViewModel.index(idx, 0), ItemSelectionModel.Select)
                            selectionModel.countSelection(true)
                        }
                    }
                    else{
                        for(let idx=selectionModel.prevSelectedIndex; idx<=index; idx++){
                            selectionModel.select(searchResultsViewModel.index(idx, 0), ItemSelectionModel.Select)
                            selectionModel.countSelection(true)
                        }
                    }
                }
                else{
                    selectionModel.select(searchResultsViewModel.index(index, 0), ItemSelectionModel.ClearAndSelect)
                    selectionModel.resetSelection()
                    selectionModel.countSelection(true)
                    selectionModel.prevSelectedIndex = index
                }
            }

            DelegateModel {
                id: visualModel
                model: currentPresetIndex !== -1 && searchResultsViewModel
                delegate: chooser

                DelegateChooser {
                    id: chooser
                    role: "resultTypeRole"

                    DelegateChoiceShot {}

                    DelegateChoicePlaylist{}

                    DelegateChoiceEdit{}

                    DelegateChoiceReference{}

                    DelegateChoiceNote{}

                    DelegateChoiceShot {
                        roleValue: "MediaAction"
                    }
                }

            }
            XsLabel {

                text: currentPresetIndex===-1 || !searchResultsViewModel ? "No query selected.\nSelect query or shot in left panel." : "No results."
                // text: "No Matching Results"
                anchors.horizontalCenter: searchResultsView.horizontalCenter
                anchors.verticalCenter: searchResultsView.verticalCenter
                // rotation: -45
                font.pixelSize: fontSize*2
                font.family: fontFamily
                font.bold: true
                color: frameColor
                scale: 0.93
                z: 1
                visible: currentPresetIndex===-1 || !searchResultsViewModel || searchResultsViewModel.sourceModel.count == 0
            }

            GridView{ id: searchResultsView
                // Rectangle{anchors.fill: parent; color: "yellow"; opacity: 0.3}

                x: framePadding
                y: framePadding
                width: parent.width - framePadding- resultScrollBar.width/2
                height: (parent.height -framePadding*3 -buttonsDiv.height)
                cellWidth: currentCategoryClass == "Edits" ? 260: (searchResultsView.width - resultScrollBar.width)
                cellHeight: (currentCategoryClass == "Edits")? (itemHeight*3 - itemSpacing)/2:
                            (currentCategoryClass == "Playlists")? (itemHeight*3 - itemSpacing)/1.15:
                            (currentCategoryClass == "Notes")? (itemHeight*7 - itemSpacing):
                            (itemHeight*3 - itemSpacing)

                clip: true
                interactive: true
                flow: GridView.FlowLeftToRight
                model: visualModel
                ScrollBar.vertical: XsScrollBar {id: resultScrollBar; anchors.right: searchResultsView.right; anchors.rightMargin: 0;
                    visible: searchResultsView.height < searchResultsView.contentHeight;
                }

                // reset scroll position on model change otherwise it'll skim the list and load tons of thumbnails
                // I don't know why this happens. Transition ?
                Connections {
                    target: searchResultsView.model
                    function onCountChanged() {
                        resultScrollBar.position = 0
                    }
                }
            }

            XsMenu { id: rightClickMenu
                width: 145

                XsMenuItem {
                    mytext: "Select All"; onTriggered: popupMenuAction(text)
                }
                XsMenuItem {
                    mytext: "Deselect All"; onTriggered: popupMenuAction(text)
                }
                XsMenuItem {
                    mytext: "Invert Selection"; onTriggered: popupMenuAction(text)
                }
                XsMenuSeparator {}
                XsMenuItem {
                    mytext: menuHistory; onTriggered: popupMenuAction(text)
                    shortcut: "V";
                    enabled: ["Notes","Versions","Reference"].includes(currentCategoryClass) && selectionModel.selectedIndexes.length
                }

                XsMenuSeparator {}

                XsMenuItem {
                    mytext: "Reveal In ShotGrid"; onTriggered: popupMenuAction(text)
                    enabled: selectionModel.selectedIndexes.length
                }
                XsMenuItem {
                    mytext: "Reveal In Ivy"; onTriggered: popupMenuAction(text)
                    enabled: selectionModel.selectedIndexes.length
                }

                XsMenuSeparator {
                    // visible: transferMenu.visible
                }
                XsMenu { id: transferMenu
                    width: 145
                    title: "Transfer Selected"
                    enabled: !["Playlists", "Notes"].includes(currentCategory)
                    // visible: enabled
                    // rightClickMenu.insertItem(index, item)

                    XsMenuItem {
                        mytext: "To Chennai"; onTriggered: popupMenuAction("Transfer chn")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuItem {
                        mytext: "To London"; onTriggered: popupMenuAction("Transfer lon")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuItem {
                        mytext: "To Montreal"; onTriggered: popupMenuAction("Transfer mtl")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuItem {
                        mytext: "To Mumbai"; onTriggered: popupMenuAction("Transfer mum")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuItem {
                        mytext: "To Sydney"; onTriggered: popupMenuAction("Transfer syd")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuItem {
                        mytext: "To Vancouver"; onTriggered: popupMenuAction("Transfer van")
                        enabled: selectionModel.selectedIndexes.length
                    }
                    XsMenuSeparator {}
                    XsMenuItem {
                        mytext: "Open Transfer Tool..."; onTriggered: popupMenuAction("maketransfer")
                        enabled: true
                    }
                }
            }

            Rectangle { id: topGradient
                width: parent.width - framePadding*2
                height: itemHeight*1
                x: framePadding
                anchors.top: parent.top
                anchors.topMargin: framePadding
                visible: searchResultsView.model.count
                opacity: 0.5 //resultScrollBar.position > 0? 0.5:0

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#77000000"}
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
            Rectangle { id: bottomGradient
                width: parent.width - framePadding*2
                height: itemHeight*1
                x: framePadding
                anchors.bottom: buttonsDiv.top
                // anchors.bottomMargin: framePadding
                visible: searchResultsView.model.count
                opacity: 0.5 //searchResultsView.contentY === searchResultsView.contentHeight - searchResultsView.height? 0:0.5

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: "#77000000" }
                }
            }
            Rectangle { id: bottomGradientDuplicate
                width: parent.width - framePadding*2
                height: itemHeight*1
                x: framePadding
                z: -1
                anchors.bottom: buttonsDiv.top
                // anchors.bottomMargin: framePadding
                visible: searchResultsView.model.count==0
                opacity: 0.5 //-bottomGradient.opacity

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: "#77000000" }
                }
            }

            Rectangle{ id: buttonsDiv
                color: "transparent"
                // x: framePadding
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottomMargin: framePadding

                anchors.leftMargin: framePadding
                anchors.rightMargin: framePadding

                width: parent.width
                height: ["Versions","Reference","Notes","Menu Setup"].includes(currentCategoryClass) ? itemHeight*2 + itemSpacing : itemHeight

                GridLayout{
                    visible: ["Versions","Reference","Notes","Menu Setup"].includes(currentCategoryClass)

                    width: parent.width
                    height: parent.height

                    flow: GridLayout.TopToBottom

                    rows: 2
                    columns: 3
                    rowSpacing: itemSpacing
                    columnSpacing: itemSpacing

                      XsButton{
                        text: "Add to New Playlist"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        onClicked: applySelection(function(items){addShotsToNewPlaylist(items)})
                        focus: false
                    }
                    XsButton{
                        text: "Add to Current Playlist"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        onClicked: applySelection(function(items){addShotsToPlaylist(items)})
                        focus: false
                    }
                    XsButton{
                        text: "Compare With Selected"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                        onClicked: applySelection(function(items){addAndCompareShotsToPlaylist(items, "A/B")})
                    }
                    XsButton{
                        text: "Compare String"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                        onClicked: applySelection(function(items){addAndCompareShotsToPlaylist(items, "String")})
                    }
                    XsButton{
                        text: "Compare Grid"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                        enabled: false
                    }
                    XsButton{
                        text: "Add to Timeline"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                        enabled: false
                    }
                }

                RowLayout{
                    visible: !["Versions","Reference","Notes","Media Actions"].includes(currentCategoryClass)
                    spacing: itemSpacing
                    anchors.fill: parent

                    XsButton{
                        visible: currentCategory == "Playlists"
                        text: "Load Playlist" + (selectionModel.selectedIndexes.length>1 ? "s" : "")
                        enabled: selectionModel.selectedIndexes.length
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                        onClicked: applySelection(
                            function(items) {
                                loadPlaylists(
                                    items,
                                    data_source.preferredVisual("Playlists"),
                                    data_source.preferredAudio("Playlists"),
                                    data_source.flagText("Playlists"),
                                    data_source.flagColour("Playlists")
                                )
                            }
                        )
                    }
                    XsButton{
                        visible: currentCategory == "Edits"
                        text: "Add to New Edit"
                        font.pixelSize: fontSize*1.2
                        Layout.preferredHeight: itemHeight
                        Layout.fillWidth: true
                        focus: false
                    }
                }
            }
        }
    }
}