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

import xStudio 1.1

ListView{
    id: queryList

    property var expanded: true

    property alias queryRootIndex: queryTreeModel.rootIndex
    property alias queryModel: queryTreeModel.baseModel

    property bool isLoaded: true
    property real queryItemHeight: itemHeight/1.15

    height: expanded ?
            queryList.model?
                ((queryItemHeight+spacing)*(queryList.model.count+1)) + framePadding
                : (queryItemHeight+spacing)+(framePadding*2)
            : 0

    Behavior on height {NumberAnimation{ duration: 150 }}

    spacing: itemSpacing
    clip: true
    orientation: ListView.Vertical
    interactive: true
    snapMode: ListView.SnapOneItem
    currentIndex: -1
    // ScrollBar.vertical: XsScrollBar{id: queryScrollBar;}
    // Keys.onPressed: (event)=> {
    //     if (event.matches(StandardKey.SelectNextLine)) {
    //         console.log("move up", currentIndex)
    //         event.accepted = true;
    //     }
    //     else if (event.matches(StandardKey.SelectPreviousLine)) {
    //         console.log("move down", currentIndex)
    //         event.accepted = true;
    //     }
    // }
    // Shortcut {
    //     sequence: StandardKey.SelectNextLine
    //     onActivated: console.log("move up", currentIndex)
    // }
    // Shortcut {
    //     sequence: StandardKey.SelectPreviousLine
    //     onActivated: console.log("move down", currentIndex)
    // }

    model: queryTreeModel

    signal snapshot()

    TreeDelegateModel {
        id: queryTreeModel

        model: null
        rootIndex:  null

        delegate:
        Item {id: queryRowItem
            property bool isEnabled: enabledRole == undefined ? false : enabledRole
            width: queryList.width
            height: queryItemHeight

            property bool held: false
            property bool wasCurrent: false
            DropArea {
                keys: ["TERM"]
                anchors { fill: parent; margins: 10 }
                onEntered: {
                    if(drag.source.DelegateModel.itemsIndex != queryRowItem.DelegateModel.itemsIndex) {
                        queryTreeModel.move(
                            drag.source.DelegateModel.itemsIndex,
                            queryRowItem.DelegateModel.itemsIndex)
                    }
                }
            }

            Keys.onPressed: (event)=> {
                if (event.matches(StandardKey.SelectNextLine)) {
                    if(currentIndex>=0) {
                        queryTreeModel.move(currentIndex, currentIndex+1)
                    }
                    event.accepted = true;
                }
                else if (event.matches(StandardKey.SelectPreviousLine)) {
                    if(currentIndex>0) {
                        queryTreeModel.move(currentIndex-1, currentIndex)
                    }
                    event.accepted = true;
                }
            }

            Rectangle{id: queryRow
                width: parent.width
                height: queryItemHeight
                color: currentIndex == index ? Qt.darker(itemColorActive, 3.75) : "transparent"

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                }
                Drag.active: queryRowItem.held
                Drag.source: queryRowItem
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.keys: ["TERM"]
                states: State {
                    when: queryRowItem.held
                    AnchorChanges {
                        target: queryRow
                        anchors { horizontalCenter: undefined; verticalCenter: undefined }
                    }
                    ParentChange { target: queryRow; parent: queryList }
                }

                RowLayout {
                    width: parent.width
                    height: parent.height
                    spacing: itemSpacing

                    Rectangle{ id: dragHandle
                        Layout.preferredWidth: framePadding*2
                        Layout.preferredHeight: queryItemHeight
                        color: "transparent"

                        Image { id: dragHandleIcon
                            x: itemSpacing
                            anchors.verticalCenter: parent.verticalCenter
                            source: "qrc:/feather_icons/align-justify.svg"
                            height: parent.height
                            width: parent.width - frameWidth*2
                            // rotation: 90
                            smooth: true
                            layer {
                                enabled: true
                                effect:
                                ColorOverlay {
                                    color: dragArea.pressed? itemColorActive: dragArea.containsMouse? textColorNormal : Qt.darker(textColorNormal, 2)
                                }
                            }
                        }

                        MouseArea{ id: dragArea
                            anchors.fill: parent
                            hoverEnabled: true
                            // onHoveredChanged: {
                            //     if(queryRowItem.containsMouse) {
                            //         queryRow.forceActiveFocus()
                            //     } else {
                            //         queryRow.focus = false
                            //     }
                            // }
                            onClicked: currentIndex = (currentIndex == index ? -1 : index)

                            drag.target: queryRowItem.held ? queryRow : undefined
                            drag.axis: Drag.YAxis
                            onPressed: {
                                queryRowItem.held = true
                                if(currentIndex == index) {
                                    currentIndex = -1
                                    queryRowItem.wasCurrent = true
                                }
                            }
                            onReleased: {
                                queryRowItem.held = false
                                if(wasCurrent) {
                                    currentIndex = index
                                    queryRowItem.wasCurrent = false
                                }
                            }
                        }

                    }

                    XsCheckbox{id: checkBox
                        // Layout.leftMargin: framePadding
                        Layout.preferredWidth: queryItemHeight
                        Layout.preferredHeight: queryItemHeight
                        imgIndicator.sourceSize.width: queryItemHeight*0.7
                        imgIndicator.sourceSize.height: queryItemHeight*0.7
                        spacing: 0

                        text: ""
                        indicatorType: "tick"
                        display: AbstractButton.IconOnly
                        checked: isEnabled
                        padding: 0
                        onClicked: {
                            enabledRole = !enabledRole
                            if(isLoaded) {
                                executeQuery()
                            }
                        }
                    }

                    XsButton {id: negation
                        property bool hasNegation: negationRole !== undefined
                        z: 1
                        subtleActive: true
                        isActive: hasNegation ? negationRole : false
                        enabled: hasNegation && isEnabled ? true : false
                        opacity: hasNegation ? 1.0 : 0.0
                        Layout.preferredWidth: queryItemHeight/2//-framePadding*1.5
                        Layout.preferredHeight: queryItemHeight//-framePadding*1.5
                        // image.sourceSize.height: height/1.1
                        // image.sourceSize.width: width/1.2
                        text: "!"
                        textColorNormal: hasNegation && negationRole ? bgColorPressed : "darkgray"
                        // imgSrc: "qrc:/feather_icons/link-2.svg"
                        // rotation: 90
                        clip: true
                        onClicked: {
                            negationRole = !negationRole
                            if(isLoaded) {
                                executeQuery()
                            }
                        }
                    }


                    XsComboBox{ id: queryBox
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: queryItemHeight
                        property var termrole: termRole

                        model: {
                            if(currentCategory == "Versions" || currentCategory == "Versions Tree")
                                return shotFilters
                            else if(currentCategory == "Playlists")
                                return playlistFilters
                            else if(currentCategory == "Edits")
                                return editFilters
                            else if(currentCategory == "Reference")
                                return referenceFilters
                            else if(currentCategory == "Menu Setup")
                                return mediaActionFilters
                            else if(currentCategory == "Notes" || currentCategory == "Notes Tree")
                                return noteFilters
                        }
                        editable: false
                        // currentIndex: {console.log(termRole, queryBox.find(termRole)); return queryBox.find(termRole)}
                        enabled: isEnabled

                        onTermroleChanged: {
                            currentIndex = queryBox.indexOfValue(termRole)
                        }

                        Component.onCompleted: {
                            currentIndex = queryBox.indexOfValue(termRole)
                        }

                        onActivated: {
                            if(currentText !== "") {
                                termRole = currentText
                                if(valueBox.count<50) valueBox.popupOptions.open()

                                if(isLoaded) {
                                    executeQuery()
                                }
                            }
                        }
                    }

                    XsButton {id: liveLink
                        property bool hasLiveLink: liveLinkRole !== undefined
                        z: 1
                        subtleActive: true
                        isActive: hasLiveLink ? liveLinkRole : false
                        enabled: hasLiveLink && isEnabled ? true : false
                        opacity: hasLiveLink ? 1.0 : 0.0
                        Layout.preferredWidth: queryItemHeight-framePadding*1.5
                        Layout.preferredHeight: queryItemHeight-framePadding*1.5
                        image.sourceSize.height: height/1.1
                        image.sourceSize.width: width/1.2
                        text: ""
                        imgSrc: "qrc:/feather_icons/link-2.svg"
                        // rotation: 90
                        clip: true
                        onClicked: {
                            liveLinkRole = !liveLinkRole
                        }
                    }

                    XsComboBox{ id: valueBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: queryItemHeight
                        property var argrole: argRole

                        editable: true
                        enabled: isEnabled && !liveLink.isActive
                        textRole: "nameRole"
                        valueRole: "nameRole"
                        textColorNormal: liveLinkRole? palette.highlight : "light grey"
                        textColorDisabled: liveLinkRole? palette.highlight : Qt.darker(textColorNormal, 1.6)
                        bgColorEditable: isEnabled && liveLink.isActive? Qt.darker(palette.highlight, 2) : "light grey"

                        onArgroleChanged: {
                            //special handling..
                            if((model == dummyModel || model == sourceModel|| termRole == "Reference Tags") && argrole != ""){
                                if(find(argrole) === -1) {
                                    let tmp = argrole
                                    model.append({nameRole: tmp})
                                }
                            }

                            if(currentIndex != find(argrole)) {
                                // could be login...
                                // dirty hack..
                                currentIndex = find(argrole)

                                if(currentIndex === -1 && model === authorModel) {
                                    // try and resolve Shotgunname..
                                    argrole = data_source.getShotgunUserName()
                                    currentIndex = find(argrole)
                                }
                                doUpdate()
                            } else if(currentIndex == 0) {
                                // model reset can cause confusion..
                                doUpdate()
                            }
                        }

                        ListModel {
                            id: dummyModel
                        }

                        ListModel {
                            id: ignoreModel

                            ListElement { nameRole: "Author" }
                            ListElement { nameRole: "Client Note" }
                            ListElement { nameRole: "Completion Location" }
                            ListElement { nameRole: "Department" }
                            ListElement { nameRole: "Filter" }
                            ListElement { nameRole: "Flag Media" }
                            ListElement { nameRole: "Has Contents" }
                            ListElement { nameRole: "Has Notes" }
                            ListElement { nameRole: "Is Hero" }
                            ListElement { nameRole: "Latest Version" }
                            ListElement { nameRole: "Lookback" }
                            ListElement { nameRole: "Exclude Shot Status" }
                            ListElement { nameRole: "Note Type" }
                            ListElement { nameRole: "On Disk" }
                            ListElement { nameRole: "Order By" }
                            ListElement { nameRole: "Pipeline Status" }
                            ListElement { nameRole: "Pipeline Step" }
                            ListElement { nameRole: "Playlist Type" }
                            ListElement { nameRole: "Playlist" }
                            ListElement { nameRole: "Preferred Audio" }
                            ListElement { nameRole: "Preferred Visual" }
                            ListElement { nameRole: "Production Status" }
                            ListElement { nameRole: "Recipient" }
                            ListElement { nameRole: "Reference Tag" }
                            ListElement { nameRole: "Reference Tags" }
                            ListElement { nameRole: "Result Limit" }
                            ListElement { nameRole: "Review Location" }
                            ListElement { nameRole: "Sent To Client" }
                            ListElement { nameRole: "Sent To Dailies" }
                            ListElement { nameRole: "Sequence" }
                            ListElement { nameRole: "Shot" }
                            ListElement { nameRole: "Shot Status" }
                            ListElement { nameRole: "Site" }
                            ListElement { nameRole: "Tag (Version)" }
                            ListElement { nameRole: "Tag" }
                            ListElement { nameRole: "Twig Name" }
                            ListElement { nameRole: "Twig Type" }
                            ListElement { nameRole: "Unit" }
                            ListElement { nameRole: "Version Name" }
                        }

                        ListModel {
                            id: sourceModel

                            ListElement { nameRole: "SG Movie" }
                            ListElement { nameRole: "SG Frames" }
                            ListElement { nameRole: "main_proxy0" }
                            ListElement { nameRole: "main_proxy1" }
                            ListElement { nameRole: "main_proxy2" }
                            ListElement { nameRole: "main_proxy3" }
                            ListElement { nameRole: "movie_client" }
                            ListElement { nameRole: "movie_dneg" }
                            ListElement { nameRole: "movie_dneg_editorial" }
                            ListElement { nameRole: "movie_scqt" }
                            ListElement { nameRole: "movie_scqt_prores" }
                            ListElement { nameRole: "orig_main_proxy0" }
                            ListElement { nameRole: "orig_main_proxy1" }
                            ListElement { nameRole: "orig_main_proxy2" }
                            ListElement { nameRole: "orig_main_proxy3" }
                            ListElement { nameRole: "review_proxy_1" }
                            ListElement { nameRole: "review_proxy_2" }
                        }

                        ListModel {
                            id: flagModel

                            ListElement { nameRole: "Red"; colour: "#FFFF0000" }
                            ListElement { nameRole: "Green"; colour: "#FF00FF00" }
                            ListElement { nameRole: "Blue"; colour: "#FF0000FF" }
                            ListElement { nameRole: "Yellow"; colour: "#FFFFFF00" }
                            ListElement { nameRole: "Orange"; colour: "#FFFFA500" }
                            ListElement { nameRole: "Purple"; colour: "#FF800080" }
                            ListElement { nameRole: "Black"; colour: "#FF000000" }
                            ListElement { nameRole: "White"; colour: "#FFFFFFFF" }
                        }

                        model: {
                            if(termRole=="Author") authorModel
                            else if(termRole=="Client Note") boolModel
                            else if(termRole=="Completion Location") primaryLocationModel
                            else if(termRole=="Department") departmentModel
                            else if(termRole=="Filter") dummyModel
                            else if(termRole=="Flag Media") flagModel
                            else if(termRole=="Has Contents") boolModel
                            else if(termRole=="Has Notes") boolModel
                            else if(termRole=="Disable Global") ignoreModel
                            else if(termRole=="Is Hero") boolModel
                            else if(termRole=="Latest Version") boolModel
                            else if(termRole=="Lookback") lookbackModel
                            else if(termRole=="Exclude Shot Status") shotStatusModel
                            else if(termRole=="Newer Version") dummyModel
                            else if(termRole=="Note Type") noteTypeModel
                            else if(termRole=="Older Version") dummyModel
                            else if(termRole=="On Disk") onDiskModel
                            else if(termRole=="Order By") orderByModel
                            else if(termRole=="Pipeline Status") pipelineStatusModel
                            else if(termRole=="Pipeline Step") stepModel
                            else if(termRole=="Playlist Type") playlistTypeModel
                            else if(termRole=="Playlist") playlistModel
                            else if(termRole=="Preferred Audio") sourceModel
                            else if(termRole=="Preferred Visual") sourceModel
                            else if(termRole=="Production Status") productionStatusModel
                            else if(termRole=="Recipient") authorModel
                            else if(termRole=="Result Limit") resultLimitModel
                            else if(termRole=="Review Location") reviewLocationModel
                            else if(termRole=="Reference Tag") referenceTagModel
                            else if(termRole=="Reference Tags") referenceTagModel
                            else if(termRole=="Sent To Client") boolModel
                            else if(termRole=="Sent To Dailies") boolModel
                            else if(termRole=="Sequence") sequenceModel
                            else if(termRole=="Shot Status") shotStatusModel
                            else if(termRole=="Shot") shotModel
                            else if(termRole=="Site") siteModel
                            else if(termRole=="Tag (Version)") dummyModel
                            else if(termRole=="Tag") dummyModel
                            else if(termRole=="Twig Name") dummyModel
                            else if(termRole=="Twig Type") twigTypeCodeModel
                            else if(termRole=="Unit") customEntity24Model
                            else if(termRole=="Version Name") dummyModel
                        }
                        Timer {
                            id: updateIndex
                            interval: 100
                            running: false
                            repeat: false
                            onTriggered: {
                                if(valueBox.model && valueBox.model.count) {
                                    valueBox.currentIndex = valueBox.indexOfValue(argRole)
                                    if(valueBox.currentIndex === -1 && valueBox.model === authorModel) {
                                        // try and resolve Shotgunname..
                                        currentIndex = valueBox.indexOfValue(data_source.getShotgunUserName())
                                        argRole = data_source.getShotgunUserName()
                                    }

                                    if(valueBox.currentIndex == -1 && !(valueBox.model == dummyModel || valueBox.model == sourceModel|| termRole == "Reference Tags") && !liveLink.isActive && queryList.expanded) {
                                        // trigger selection..
                                        valueBox.popupOptions.open()
                                    }
                                } else {
                                    updateIndex.start()
                                }
                            }
                        }

                        Connections {
                            target: queryList
                            function onExpandedChanged() {
                                queryList.expanded && valueBox.currentIndex == -1 && updateIndex.start()
                            }
                        }

                        onAccepted: {
                            // special handling for Name text
                            if((model == dummyModel || model == sourceModel || termRole == "Reference Tags") && find(editText) === -1) {
                                if(editText != "") {
                                    model.append({nameRole: editText})
                                }
                            }
                            focus=false
                            doUpdate()
                        }

                        Component.onCompleted: {
                            if(!(model == dummyModel || model == sourceModel || termRole == "Reference Tags"))
                                updateIndex.start()
                            else {
                                model.append({nameRole: argRole})
                                currentIndex = find(argRole)
                            }
                        }

                        onFocusChanged: if(!focus) accepted()

                        function doUpdate() {
                            // console.log(argRole, currentText)
                            if(currentText !== "" && argRole != currentText) {
                                if(termRole == "Reference Tags" && !currentText.includes(",")) {
                                    // split..
                                    let items = argRole.split(",")
                                    if(items.includes(currentText)) {
                                        items.splice(items.indexOf(currentText),1)
                                    } else {
                                        items.push(currentText)
                                    }
                                    // filter empty items.
                                    items = items.filter(Boolean)

                                    argRole = items.join(",")
                                }
                                else
                                    argRole = currentText
                            }

                            if(isLoaded) {
                                executeQuery()
                            }
                        }

                        onActivated: {
                            doUpdate()
                        }
                    }

                    XsButton{id: deleteButton
                        Layout.preferredWidth: queryItemHeight
                        Layout.preferredHeight: queryItemHeight
                        // Layout.rightMargin: framePadding

                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        onClicked: {
                            snapshot()
                            queryTreeModel.remove(index)
                            snapshot()
                            if(isLoaded) {
                                executeQuery()
                            }
                        }
                    }
                }

            }
        }
    }

    footer:
        ColumnLayout {
            width: queryList.width
            height: queryItemHeight+itemSpacing*2
            anchors.topMargin: itemSpacing*2

            XsComboBox{
                id: selectField
                Layout.preferredWidth: 150
                Layout.preferredHeight: queryItemHeight
                Layout.leftMargin: (queryItemHeight/2)+itemSpacing+(queryItemHeight+itemSpacing) + (framePadding*2+itemSpacing)

                model: {
                    if(currentCategory == "Versions" || currentCategory == "Versions Tree")
                        return ["-- Select --"].concat(shotFilters)
                    else if(currentCategory == "Playlists")
                        return ["-- Select --"].concat(playlistFilters)
                    else if(currentCategory == "Edits")
                        return ["-- Select --"].concat(editFilters)
                    else if(currentCategory == "Reference")
                        return ["-- Select --"].concat(referenceFilters)
                    else if(currentCategory == "Menu Setup")
                        return ["-- Select --"].concat(mediaActionFilters)
                    else if(currentCategory == "Notes" || currentCategory == "Notes Tree")
                        return ["-- Select --"].concat(noteFilters)
                }

                onModelChanged: currentIndex = 0
                currentIndex: 0

                onActivated: {
                    if(currentIndex !== -1 && currentIndex !== 0) {
                        let value = ""
                        let id = ""
                        if(selectField.currentText == "Author") {
                            value = data_source.getShotgunUserName()
                        }
                        else if(selectField.currentText == "Recipient") {
                            value = data_source.getShotgunUserName()
                        }
                        else if(selectField.currentText == "Site")
                            value = helpers.getEnv("DNSITEDATA_SHORT_NAME", "")
                        else if(selectField.currentText == "Has Contents")
                            value = "True"
                        else if(selectField.currentText == "Sent To Client")
                            value = "True"
                        else if(selectField.currentText == "Sent To Dailies")
                            value = "True"
                        else if(selectField.currentText == "Has Notes")
                            value = "True"
                        else if(selectField.currentText == "Latest Version")
                            value = "True"

                        snapshot()

                        let term = {"term": selectField.currentText, "value": value, "enabled": true}

                        // only certain terms can be pinned..
                        if(["Older Version", "Newer Version", "Version Name", "Author", "Recipient", "Shot", "Pipeline Step", "Twig Name", "Twig Type", "Sequence"].includes(selectField.currentText)) {
                            term["livelink"] = false
                        }

                        if(["Pipeline Step", "Playlist Type", "Site", "Department",
                            "Filter", "Tag", "Unit", "Note Type","Version Name", "Pipeline Status",
                            "Production Status", "Shot Status", "Twig Type", "Twig Name", "Shot Status",
                            "Tag (Version)", "Reference Tag", "Reference Tags", "Twig Name", "Completion Location", "On Disk"].includes(selectField.currentText)) {
                            term["negated"] = false
                        }

                        queryTreeModel.insert(queryTreeModel.count, term)
                        snapshot()

                        currentIndex = 0
                        // update query on change.
                        if(isLoaded && value != "") {
                            executeQuery()
                        }
                    }
                }
            }
        }
}

