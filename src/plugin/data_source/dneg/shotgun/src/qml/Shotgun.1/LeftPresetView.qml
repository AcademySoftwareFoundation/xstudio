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


Rectangle{ id: presetsDiv
    property alias presetSelectionModel: presetSelectionModel
    property alias presetsModel: searchPresetsView.delegateModel
    property alias searchPresetsView: searchPresetsView
    property real presetTitleHeight: itemHeight
    property var backendModel//: searchPresetsViewModel
    property bool isActiveView: true

    color: "transparent"
    border.color: frameColor
    border.width: frameWidth
    // focus: true

    ItemSelectionModel {
        id: presetSelectionModel
        model: backendModel
    }

    Component {
        id: dragDelegate
        // MouseArea
        MouseArea {
            id: dragArea

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            property var model: DelegateModel.model
            property bool presetLoadedRole: backendModel.activePreset === index && isActiveView
            property bool isMouseHovered: containsMouse || (editMenu.visible && searchPresetsView.menuActionIndex==index)
            property bool held: false
            property bool was_current: false

            hoverEnabled: true

            anchors { left: parent ? parent.left : undefined; right: parent ? parent.right : undefined}

            height: content.height
            width: searchPresetsView.width

            drag.target: held ? content : undefined
            drag.axis: Drag.YAxis

            onDoubleClicked: {
                presetSelectionModel.select(presetsModel.modelIndex(index), ItemSelectionModel.ClearAndSelect)
                if(presetsModel.model.activePreset == index)
                    presetsModel.model.activePreset = -1
                presetsModel.model.activePreset = index
            }

            onClicked: {
                if(mouse.modifiers == Qt.NoModifier) {
                    if(!held) {
                        if (mouse.button == Qt.RightButton) {
                            searchPresetsView.menuActionIndex = index
                            editMenu.popup()
                        } else {
                            presetSelectionModel.select(presetsModel.modelIndex(index), ItemSelectionModel.ClearAndSelect)
                            presetsModel.model.activePreset = index
                        }
                    }
                } else if(mouse.button == Qt.LeftButton){
                    if(mouse.modifiers == Qt.ShiftModifier){
                        // Only works with one current selection.
                        if(presetSelectionModel.selectedIndexes.length==1) {
                            let s = Math.min(presetSelectionModel.selectedIndexes[0].row,index)
                            let e = Math.max(presetSelectionModel.selectedIndexes[0].row,index)
                            for(;s<=e;s++){
                                presetSelectionModel.select(presetsModel.modelIndex(s), ItemSelectionModel.Select)
                            }
                        }
                    } else if(mouse.modifiers == Qt.ControlModifier) {
                        presetSelectionModel.select(presetsModel.modelIndex(index), ItemSelectionModel.Toggle)
                    }
                }
            }

            onPressAndHold: {
                held = true
                if(presetsModel.model.activePreset == index) {
                    presetsModel.model.activePreset = -1
                    was_current = true
                }
            }
            onReleased: {
                held = false
                if(was_current) {
                    was_current = false
                    presetsModel.model.activePreset = index
                }
            }

            DropArea {
                keys: ["PRESET"]
                anchors { fill: parent; margins: 10 }

                onEntered: {
                    // if(drag.source.DelegateModel.itemsIndex != dragArea.DelegateModel.itemsIndex) {
                        // presetsModel.items.move(
                        //     drag.source.DelegateModel.itemsIndex,
                        //     dragArea.DelegateModel.itemsIndex)

                        presetsModel.move(
                            drag.source.DelegateModel.itemsIndex,
                            dragArea.DelegateModel.itemsIndex)
                    // }
                }
            }
            // rectangle
            Rectangle {
                // Preset title bar
                id: content
                color: "transparent"

                height: (presetTitleHeight+ searchQueryView.height)
                width: dragArea.width

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                }

                Drag.active: dragArea.held
                Drag.source: dragArea
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.keys: ["PRESET"]

                states: State {
                    when: dragArea.held

                    ParentChange { target: content; parent: presetsDiv }
                    AnchorChanges {
                        target: content
                        anchors { horizontalCenter: undefined; verticalCenter: undefined }
                    }
                }

                // we need to somehow update these...
                Connections {
                    target: searchQueryView.queryModel
                    function onJsonChanged() {
                        searchQueryView.queryRootIndex = model.modelIndex(index, model.rootIndex)
                    }
                }


                Component.onCompleted: {
                    searchQueryView.queryModel = model.model
                    searchQueryView.queryRootIndex = model.modelIndex(index, model.rootIndex)
                }

                XsMenu { id: editMenu
                    width: 150

                    XsMenuItem {
                        mytext: "Rename..."; onTriggered: presetsDiv.onMenuAction("RENAME")
                    }
                    XsMenuItem {
                        mytext: "Duplicate"; onTriggered: presetsDiv.onMenuAction("DUPLICATE")
                    }
                    MenuSeparator{ }
                    XsMenuItem {
                        mytext: "Copy Preset"; onTriggered: presetsDiv.onMenuAction("COPY", index)
                        // shortcut: "Ctrl+C"
                    }
                    MenuSeparator{ }
                    XsMenuItem {
                        mytext: "Remove"; onTriggered: presetsDiv.onMenuAction("REMOVE", index)
                    }
                }

                Rectangle{
                    id: presetNameDiv
                    property bool divSelected: false
                    property int slNumber: index+1

                    Connections {
                        target: presetSelectionModel
                        function onSelectionChanged(selected, deselected) {
                            presetNameDiv.divSelected = presetSelectionModel.rowIntersectsSelection(index , presetsModel.parentModelIndex())
                        }
                    }

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                    }

                    width: parent.width
                    height: presetTitleHeight
                    color: {
                        if(held)
                            Qt.darker(itemColorActive, 3.75)
                        else {
                            backendModel.activePreset == index || divSelected ? Qt.darker(itemColorActive, 2.75) : itemColorNormal
                        }
                    }
                    border.color: isMouseHovered? itemColorActive: itemColorNormal

                    Rectangle{ id: activeIndicator
                        visible: backendModel.activePreset == index
                        z: 10
                        width: framePadding/1.5; height: parent.height
                        anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                        color: itemColorActive
                    }

                    XsText{
                        text: !isCollapsed? nameRole : nameRole.substr(0,2)+".."
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            verticalCenter: parent.verticalCenter
                        }
                        elide: Text.ElideRight
                        width: isCollapsed? parent.width: parent.width - framePadding*2
                        height: parent.height
                        font.pixelSize: fontSize*1.2
                        font.weight: presetLoadedRole? Font.DemiBold : Font.Normal
                        color: isMouseHovered || presetLoadedRole? textColorActive: textColorNormal
                        horizontalAlignment: Text.AlignHCenter // Text.AlignLeft // Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        ToolTip.text: nameRole
                        ToolTip.visible: isMouseHovered && isCollapsed
                        visible: !(searchPresetsView.isEditable && searchPresetsView.menuActionIndex == index)
                    }

                    XsTextField{
                        id: textField
                        text: nameRole
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            verticalCenter: parent.verticalCenter
                        }
                        width: parent.width
                        height: parent.height
                        font.pixelSize: fontSize*1.2
                        color: isMouseHovered || presetLoadedRole? textColorActive: textColorNormal
                        bgVisibility: textField.focus
                        visible: searchPresetsView.isEditable && searchPresetsView.menuActionIndex == index
                        focus: visible

                        onFocusChanged: {
                            if(focus) {
                                forceActiveFocus()
                                selectAll()
                            }
                        }

                        onEditingCompleted: {
                            searchPresetsView.isEditable = false
                            searchPresetsView.menuActionIndex = -1
                            nameRole = text
                        }

                        ToolTip.text: nameRole
                        ToolTip.visible: isMouseHovered && isCollapsed
                    }


                    RadialGradient { id: overlayBg
                        visible: !isCollapsed && (isMouseHovered)
                        anchors.centerIn: parent
                        width:parent.width - frameWidth*2
                        height: parent.height - frameWidth*2
                        angle: 0
                        horizontalOffset: 0
                        horizontalRadius: width/2
                        verticalOffset: 0
                        verticalRadius: height
                        gradient:
                        Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: "transparent" }
                            GradientStop { position: 0.85; color: presetNameDiv.color }
                        }
                    }
                    XsButton{id: expandButton
                        property bool isExpanded: expandedRole

                        text: ""
                        imgSrc: "qrc:/feather_icons/chevron-down.svg"
                        visible: !isCollapsed && (isMouseHovered || hovered || isActive)
                        width: height
                        height: parent.height - framePadding
                        image.sourceSize.width: height/1.3
                        image.sourceSize.height: height/1.3
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: framePadding +frameWidth/2
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        isActive: isExpanded

                        scale: rotation==0 || rotation==-90?1:0.85
                        rotation: (isExpanded)? 0: -90
                        Behavior on rotation {NumberAnimation{id: rotationAnim; duration: 150 }}

                        TapHandler {
                            acceptedModifiers: Qt.ShiftModifier
                            onTapped: {
                                expandedRole = !expandedRole
                            }
                        }
                        TapHandler {
                            acceptedModifiers: Qt.NoModifier
                            onTapped: {
                                if(!expandedRole) {
                                    presetsModel.model.clearExpanded()
                                }
                                expandedRole = !expandedRole
                            }
                        }
                    }
                    Rectangle{ anchors{right: busy.right; verticalCenter: busy.verticalCenter; }
                        width: busy.width; height: busy.height; opacity: busy.visible? 0.7:0
                        Behavior on opacity {NumberAnimation{duration: 150 }}
                        color: parent.color; radius: width/2;
                    }
                    XsBusyIndicator {
                        id: busy
                        running: false
                        width: 40
                        height: parent.height - framePadding
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: expandButton.right
                        anchors.leftMargin: itemSpacing
                    }

                    Connections {
                        target: leftDiv
                        function onBusyQuery(queryIndex, isBusy){
                            if(queryIndex == index)
                                busy.running=isBusy
                        }
                    }

                    XsButton{id: deleteButton

                        text: ""
                        imgSrc: "qrc:/feather_icons/x.svg"
                        visible: !isCollapsed && (isMouseHovered || hovered)
                        width: height
                        height: parent.height - framePadding
                        image.sourceSize.width: height/1.3
                        image.sourceSize.height: height/1.3
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: moreButton.left
                        anchors.rightMargin: itemSpacing
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)

                        onClicked: presetsDiv.onMenuAction("REMOVE", index)
                    }
                    XsButton{id: moreButton

                        text: ""
                        imgSrc: "qrc:/feather_icons/more-vertical.svg"
                        visible: !isCollapsed && (isMouseHovered || hovered || isActive)
                        width: height
                        height: parent.height - framePadding
                        image.sourceSize.width: height/1.3
                        image.sourceSize.height: height/1.3
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: framePadding +frameWidth
                        borderColorNormal: Qt.lighter(itemColorNormal, 0.3)
                        isActive: (editMenu.visible && searchPresetsView.menuActionIndex==index)

                        onClicked: {
                            searchPresetsView.menuActionIndex = index
                            editMenu.popup()
                        }

                    }

                    // trigger after timer.

                    // Component.onCompleted: {
                    //     if(presetLoadedRole)
                    //         searchPresetsView.currentIndex = index
                    // }
                }

                // Preset TERMS
                QueryListView {
                    id: searchQueryView

                    x: frameWidth
                    y: presetTitleHeight + framePadding
                    width: parent.width - frameWidth*4 -framePadding

                    interactive: true
                    snapMode: ListView.SnapToItem
                    onSnapshot: snapshotPresets()
                    expanded: expandedRole
                    isLoaded: presetLoadedRole
                    queryRootIndex: null
                    queryModel: null
                }
            }
            // rectangle
        }
        // MouseArea
    }

    XsTreeStructure{ id: searchPresetsView
        property alias treeView: searchPresetsView
        spacing: itemSpacing
        snapMode: ListView.SnapToItem
        scrollBar.visible: !searchShotListPopup.visible && searchPresetsView.height < searchPresetsView.contentHeight

        width: parent.width
        height:  isCollapsed? parent.height : parent.height - (savePresetDiv.height + savePresetDiv.anchors.bottomMargin*2)

        delegateModel.model: backendModel
        delegateModel.delegate: dragDelegate

        Connections {
            target: backendModel
            function onActivePresetChanged() {
                searchPresetsView.currentIndex = backendModel.activePreset
                searchPresetsView.isEditable = false
                searchPresetsView.menuActionIndex = -1
                if(backendModel.activePreset != -1 && backendModel.get(backendModel.activePreset).startsWith("Live "))
                    clearFilter()
                presetSelectionModel.select(presetsModel.modelIndex(backendModel.activePreset), ItemSelectionModel.ClearAndSelect)
                executeQuery()
            }
        }

        MouseArea{ id: focusMArea; width: parent.width; height: parent.height-parent.contentHeight;
            anchors.bottom: parent.bottom; onPressed: focus= true; onReleased: focus= false;
        }

    }



    function duplicatePreset(index, title) {
        presetsModel.insert(presetsModel.count, presetsModel.get(index, "jsonRole"))
        presetsModel.set(presetsModel.count-1, title, "nameRole")
        presetsModel.model.setType(presetsModel.count-1)

        return presetsModel.count-1
    }

    function onMenuAction(actionText, index=-1)
    {
        if(actionText == "RESET")
        {
            snapshotPresets()
            data_source.resetPreset(currentCategory)
            clearResults()
            // if(index !== -1) {
            //     data_source.resetPreset(currentCategory, index)
            // } else{
            //     for(let i=0;i<presetSelectionModel.selectedIndexes.length;i++) {
            //         data_source.resetPreset(currentCategory, presetSelectionModel.selectedIndexes[i].row)
            //     }
            // }
            snapshotPresets()
        }
        else if(actionText == "RENAME")
        {
            searchPresetsView.isEditable = true
        }
        else if(actionText == "COPY")
        {
            let a = []
            if(index !== -1) {
                a.push(presetsModel.get(index, "jsonTextRole"))
            } else {
                for(let i=0;i<presetSelectionModel.selectedIndexes.length;i++) {
                    a.push(presetsModel.get(presetSelectionModel.selectedIndexes[i].row, "jsonTextRole"))
                }
            }
            clipboard.text = "[" + a.join() +"]"
        }
        else if(actionText == "PASTE")
        {
            snapshotPresets()
            try {
                let j = JSON.parse(clipboard.text)
                for(let i=0;i < j.length; i++) {
                    presetsModel.insert(presetsModel.count, j[i])
                    presetsModel.model.setType(presetsModel.count-1)
                }
            } catch(err){
                console.log(actionText+"_Err: "+err)
            }
            snapshotPresets()
        }
        else if(actionText == "DUPLICATE")
        {
            snapshotPresets()
            index = searchPresetsView.menuActionIndex
            let newind = duplicatePreset(index, presetsModel.get(index, "nameRole") + " - copy")
            searchPresetsView.currentIndex = newind
            snapshotPresets()
        }
        else if(actionText == "REMOVE")
        {
            snapshotPresets()
            if(index !== -1) {
                if(presetsModel.model.activePreset == index) {
                    clearResults()
                }
                presetsModel.remove(index)
            } else {
                // process in reverse order..
                let to_remove = []
                for(let i=0;i<presetSelectionModel.selectedIndexes.length;i++) {
                    to_remove.push(presetSelectionModel.selectedIndexes[i].row)
                }
                to_remove.sort(function(a, b){return b - a}); //sorts numerically in descending order
                for(let i=0;i<to_remove.length;i++) {
                    if(presetsModel.model.activePreset == to_remove[i]) {
                        clearResults()
                    }
                    presetsModel.remove(to_remove[i])
                }
            }
            snapshotPresets()
        } else {
            console.log("Unknown action", actionText)
        }
    }

    Connections {
        target: rightDiv
        function onCreatePreset(query, value) {
            snapshotPresets()
            let title = (value == "True" ? query : value) + " - Versions"
            let id = value

            let newpreset_index = presetsModel.model.activePreset

            presetsModel.set(newpreset_index, title, "nameRole")

            let child_index = presetsModel.modelIndex(newpreset_index)

            if(query == "Author") {
                id = authorModel.get(authorModel.search(value),"idRole")
            } else if(query == "Shot") {
                id = shotModel.get(shotModel.search(value),"idRole")
            }

            presetsModel.append({"term": query,  "value": value, "id": id, "enabled": true}, child_index)
            if(presetsModel.model.activePreset == newpreset_index) {
                // force update
                executeQuery()
            } else {
                presetsModel.model.activePreset = newpreset_index
            }
            snapshotPresets()
        }

        function onForceSelectPreset(index) {
            presetsModel.model.activePreset = index
        }
    }

    Rectangle { id: bottomGradient
        visible: !isCollapsed
        width: parent.width
        height: itemHeight
        anchors.bottom: savePresetDiv.top
        // anchors.bottomMargin: framePadding
        opacity: 0.5

        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#77000000"}
        }
    }

    Item{id: savePresetDiv
        visible: !isCollapsed
        x: framePadding
        width: parent.width -framePadding*2
        height: isCollapsed? 0 : itemHeight//+itemSpacing
        anchors.bottom: parent.bottom
        anchors.bottomMargin: framePadding

        RowLayout{
            spacing: itemSpacing
            anchors.fill: parent

            XsButton{id: savePresetButton
                text: "New Preset"
                Layout.fillWidth: true
                Layout.preferredHeight: itemHeight
                font.pixelSize: fontSize*1.2
                onClicked: {
                    presetsModel.insert(presetsModel.count, {"name": "Untitled", "queries": [ ], "expanded": true} )
                    searchPresetsView.menuActionIndex = presetsModel.count-1
                    presetsDiv.onMenuAction("RENAME")
                }
            }

            XsButton{id: moreActionsButton
                text: ""
                imgSrc: "qrc:/feather_icons/more-vertical.svg"
                Layout.preferredWidth: itemHeight
                Layout.preferredHeight: itemHeight
                font.pixelSize: fontSize*1.2
                isActive: moreActionsMenu.opened
                onClicked: {
                    moreActionsMenu.popup()
                }
            }
        }

        XsMenu { id: moreActionsMenu
            width: 200

            XsMenuItem {
                mytext: "Copy Selected Presets"; onTriggered: {searchPresetsView.menuActionIndex = presetsModel.model.activePreset; presetsDiv.onMenuAction("COPY")}
                shortcut: "Ctrl+C";
                enabled: presetSelectionModel.selectedIndexes.length > 0
            }
            XsMenuItem {
                mytext: "Paste Presets"; onTriggered: presetsDiv.onMenuAction("PASTE")
                shortcut: "Ctrl+V";
                enabled: clipboard.text !== ""
            }
            MenuSeparator{ }
            XsMenuItem {
                mytext: "Undo"; onTriggered: undo()
                shortcut: "Ctrl+Z"
            }
            XsMenuItem {
                mytext: "Redo"; onTriggered: redo()
                shortcut: "Ctrl+Shift+Z"
            }
            MenuSeparator{ }
            XsMenuItem {
                mytext: "Reset Default Presets"; onTriggered: presetsDiv.onMenuAction("RESET")
                // enabled: presetSelectionModel.selectedIndexes.length > 0
            }
            MenuSeparator{ }
            XsMenuItem {
                mytext: "Remove Selected Presets"; onTriggered: presetsDiv.onMenuAction("REMOVE")
                enabled: presetSelectionModel.selectedIndexes.length > 0
            }
        }
    }

    Rectangle { id: overlapGradient
        visible: searchShotListPopup.visible
        onVisibleChanged: {
            if(visible){
                searchPresetsView.enabled = false
                savePresetDiv.enabled = false
            }
            if(!visible){
                searchPresetsView.enabled = true
                savePresetDiv.enabled = true
            }
        }
        anchors.fill: parent
        MouseArea{anchors.fill: parent;onClicked: searchTextField.focus = false}

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#77000000" }
            GradientStop { position: 0.8; color: "#33000000" }
            GradientStop { position: 1.0; color: "transparent"}
        }
    }
}