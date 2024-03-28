// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick 2.14
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import QtQml 2.12
import Qt.labs.qmlmodels 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1
import xstudio.qml.helpers 1.0

Rectangle {
    id: timeline
    color: timelineBackground

    property var hovered: null
    property real scaleX: 3.0
    property real scaleY: 1.0
    property real itemHeight: 30.0
    property real trackHeaderWidth: 200.0

    property color timelineBackground: "#FF333333"
    property color timelineText: "#FFAFAFAF"
    property color trackBackground: "#FF474747"
    property color trackEdge: "#FF5B5B5B"
    property color defaultClip: "#FF595959"

    focus: true
    property alias timelineSelection: timelineSelection
    property alias timelineFocusSelection: timelineFocusSelection
    // onActiveFocusChanged: {
    //     console.log("onActiveFocusChanged", activeFocusItem)
    //     forceActiveFocus()
    // }

    signal jumpToStart()
    signal jumpToEnd()
    signal jumpToFrame(int frame, bool center)

    DelegateChooser {
        id: chooser
        role: "typeRole"

        XsDelegateStack {}
    }

    DelegateModel {
        id: timeline_items
        property var srcModel: app_window.sessionModel
        model: srcModel
        rootIndex: null
        delegate: chooser
    }

    ItemSelectionModel {
        id: timelineSelection
        model: timeline_items.srcModel
    }

    ItemSelectionModel {
        id: timelineFocusSelection
        model: timeline_items.srcModel

        onSelectionChanged: focusItems(timelineFocusSelection.selectedIndexes)
    }

    Connections {
      target: app_window.currentSource
      function onIndexChanged() {
        if(app_window.currentSource.values.typeRole == "Timeline") {
            forceActiveFocus()
            timeline_items.rootIndex = app_window.sessionModel.index(2, 0, app_window.currentSource.index)
            startFrames.index = app_window.sessionModel.index(2, 0, app_window.currentSource.index)
        }
        // else
        //   items.rootIndex = app_window.sessionModel.index(-1,-1)
      }
    }

    XsModelProperty {
        id: startFrames
        role: "trimmedStartRole"
        index: app_window.sessionModel.index(2, 0, app_window.currentSource.index)
    }

    XsStringRequestDialog {
        id: set_name_dialog
        title: "Change Name"
        okay_text: "Set Name"
        text: "Tag"
        property var index: null
        onOkayed: setItemName(index, text)
    }

    XsButtonDialog {
        id: new_item_dialog
        rejectIndex: 0
        acceptIndex: -1
        width: 500
        text: "Choose item to add"
        title: "Add Timeline Item"
        property var insertion_parent: null
        property int insertion_row: 0

        buttonModel: ["Cancel", "Clip", "Gap", "Audio Track", "Video Track", "Stack"]
        onSelected: {
            if(button_index != 0)
                addItem(buttonModel[button_index], insertion_parent, insertion_row)
        }
    }

    function setTrackHeaderWidth(val) {
        trackHeaderWidth = Math.max(val, 40)
    }

    function addItem(type, insertion_parent, insertion_row) {

        // insertion type
        let insertion_index_type = app_window.sessionModel.get(insertion_parent, "typeRole")
        if(type == "Video Track") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = app_window.sessionModel.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = app_window.sessionModel.index(0, 0, insertion_parent) // stack
                insertion_row = 0
            } else if(insertion_index_type != "Stack") {
                insertion_parent = null
            }
        }
        else if(type == "Audio Track") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = app_window.sessionModel.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = app_window.sessionModel.index(0, 0, insertion_parent) // stack
                insertion_row = app_window.sessionModel.rowCount(insertion_parent) // last track + 1
            } else if(insertion_index_type != "Stack") {
                insertion_parent = null
            }
        }
        else if(type == "Gap" || type == "Clip") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = app_window.sessionModel.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = app_window.sessionModel.index(0, 0, insertion_parent) // stack
                insertion_parent = app_window.sessionModel.index(0, 0, insertion_parent) // track
                insertion_row = app_window.sessionModel.rowCount(insertion_parent) // last clip
            } else if (insertion_index_type == "Stack") {
                insertion_parent = app_window.sessionModel.index(insertion_row, 0, insertion_parent)
                insertion_row = app_window.sessionModel.rowCount(insertion_parent)
            } else {
                console.log(insertion_parent, insertion_index_type)
            }
        }

        if(insertion_parent != null) {
            app_window.sessionModel.insertRowsSync(insertion_row, 1, type, "New Item", insertion_parent)
        }
    }

    Connections {
        target: app_window
        function onFlagSelectedItems(flag) {
            if(timeline.visible) {
                let indexes = timelineSelection.selectedIndexes
                for(let i=0;i<indexes.length; i++) {
                    app_window.sessionModel.set(indexes[i], flag, "flagColourRole")
                }
            }
        }
    }

    function addGap(parent, row, name = "NewGap", frames=24, rate=24.0) {
        return app_window.sessionModel.insertTimelineGap(row, parent, frames, rate, name)
    }

    function addClip(parent, row, media_index,  name = "New Clip") {
        return app_window.sessionModel.insertTimelineClip(row, parent, media_index, name)
    }

    function deleteItems(indexes) {
        app_window.sessionModel.removeTimelineItems(indexes);
    }

    function deleteItemFrames(index, start, duration) {
        app_window.sessionModel.removeTimelineItems(index, start, duration);
    }

    function undo(timeline_index) {
        app_window.sessionModel.undo(timeline_index)
    }

    function redo(timeline_index) {
        app_window.sessionModel.redo(timeline_index)
    }

    function moveItem(index, distance) {
        app_window.sessionModel.moveTimelineItem(index, distance)
    }

    function focusItems(items) {
        app_window.sessionModel.setTimelineFocus(timeline_items.rootIndex, items)
    }

    function rightAlignItems(indexes) {
        app_window.sessionModel.alignTimelineItems(indexes, true)
    }

    function leftAlignItems(indexes) {
        app_window.sessionModel.alignTimelineItems(indexes, false)
    }

    function moveItemFrames(index, start, duration, dest, insert) {
        app_window.sessionModel.moveRangeTimelineItems(index, start, duration, dest, insert)
    }

    function enableItems(indexes, enabled) {
        for(let i=0;i<indexes.length; i++) {
            app_window.sessionModel.set(indexes[i], enabled, "enabledRole")
        }
    }

    function setItemName(index, name) {
        app_window.sessionModel.set(index, name, "nameRole")
    }

    function splitClip(index, frame) {
        return app_window.sessionModel.splitTimelineClip(frame, index)
    }

    function handleDrop(before, drop) {
        if(drop.hasUrls) {
            for(var i=0; i < drop.urls.length; i++) {
                if(drop.urls[i].toLowerCase().endsWith('.xst') || drop.urls[i].toLowerCase().endsWith('.xsz')) {
                    // Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
                    // app_window.sessionFunction.newRecentPath(drop.urls[i])
                    return;
                }
            }
        }

        // prepare drop data
        let data = {}
        for(let i=0; i< drop.keys.length; i++){
            data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
        }

        if(before.valid) {
            if("xstudio/media-ids" in data) {
                let internal_copy = false

                // does media exist in our parent.
                if(before) {
                    let mi = app_window.sessionModel.search_recursive(
                        helpers.QVariantFromUuidString(data["xstudio/media-ids"].split("\n")[0]), "idRole"
                    )

                    if(app_window.sessionModel.getPlaylistIndex(before) == app_window.sessionModel.getPlaylistIndex(mi)) {
                        internal_copy = true
                    }
                }

                if(internal_copy) {
                    Future.promise(
                        app_window.sessionModel.handleDropFuture(Qt.CopyAction, data, before)
                    ).then(function(quuids){})
                } else {
                    media_move_copy_dialog.data = data
                    media_move_copy_dialog.index = before
                    media_move_copy_dialog.open()
                }

            } else if("xstudio/timeline-ids" in data) {
                let internal_copy = false

                // does media exist in our parent.
                if(before) {
                    let mi = app_window.sessionModel.search_recursive(
                        helpers.QVariantFromUuidString(data["xstudio/timeline-ids"].split("\n")[0]), "idRole"
                    )

                    if(app_window.sessionModel.getPlaylistIndex(before) == app_window.sessionModel.getPlaylistIndex(mi)) {
                        internal_copy = true
                    }
                }

                if(internal_copy) {
                    // force move action..
                    Future.promise(
                        app_window.sessionModel.handleDropFuture(Qt.MoveAction, data, before)
                    ).then(function(quuids){})
                } else {
                    console.log("external copy")
                    // items from external timeline
                }
            } else {
                Future.promise(
                    app_window.sessionModel.handleDropFuture(drop.proposedAction, data, before)
                ).then(function(quuids){})
            }
        }
    }

    function resolveItem(x, y) {
        let local_pos = mapToItem(list_view, x, y)
        let item = list_view.itemAt(local_pos.x, local_pos.y)
        let item_type = null
        let local_x = 0
        let local_y = 0

        function _resolveItem(item, local_x, local_y) {
            let item_type = null

            if(item) {
                item_type = item.itemTypeRole
                if(! ["Clip", "Gap"].includes(item_type)) {
                    // check for sub child.
                    let child_item = null
                    if(["Stack"].includes(item_type)) {

                        if(local_y < item.timelineHeaderHeight)
                            return [item, item_type, local_x, local_y]

                        let listview_pos = item.mapToItem(item.list_view_video, local_x, local_y)
                        child_item = item.list_view_video.itemAt(listview_pos.x + item.list_view_video.contentX , listview_pos.y + item.list_view_video.contentY)

                        if(child_item == null) {
                            listview_pos = item.mapToItem(item.list_view_audio, local_x, local_y)
                            child_item = item.list_view_audio.itemAt(listview_pos.x + item.list_view_audio.contentX , listview_pos.y + item.list_view_audio.contentY)
                        }

                    } else {
                        let listview_pos = item.mapToItem(item.list_view, local_x, local_y)

                        if(["Video Track", "Audio Track"].includes(item_type)  && local_x < trackHeaderWidth)
                            return [item, item_type, local_x, local_y]

                        child_item = item.list_view.childAt(listview_pos.x, listview_pos.y)
                        // child_item = item.list_view.childAt(listview_pos.x + item.list_view.contentX , listview_pos.y + item.list_view.contentY)
                    }

                    if(child_item) {
                        let child_item_pos = item.mapToItem(child_item, local_x, local_y)
                        return _resolveItem(child_item, child_item_pos.x, child_item_pos.y)
                    }
                }
            }

            return [item, item_type, local_x, local_y]
        }

        if(item) {
            local_pos = mapToItem(item, x, y)

            let [_item, _item_type, _local_x, _local_y] = _resolveItem(item, local_pos.x, local_pos.y)
            item = _item
            item_type = _item_type
            local_x = _local_x
            local_y = _local_y
        }

        return [item, item_type, local_x, local_y]
    }

    function anteceedingIndex(item_index) {
        let result = item_index.model.index(-1,-1,item_index.parent)
        let mi_row = item_index.row
        let count = item_index.model.rowCount(item_index.parent)

        if(count != 1 && mi_row + 1 < count) {
            result = item_index.model.index(mi_row + 1, 0, item_index.parent)
        }

        return result;
    }

    function preceedingIndex(item_index) {
        let result = item_index.model.index(-1,-1,item_index.parent)
        let mi_row = item_index.row
        let count = item_index.model.rowCount(item_index.parent)

        if(count != 1 && mi_row) {
            result = item_index.model.index(mi_row - 1, 0, item_index.parent)
        }

        return result;
    }

    XsMenu {
        id: timelineMenu
        title: qsTr("Timeline")

        XsMenuItem {
            mytext: qsTr("Set Focus")
            onTriggered: timelineFocusSelection.select(helpers.createItemSelection(timelineSelection.selectedIndexes), ItemSelectionModel.ClearAndSelect)
        }

        XsMenuItem {
            mytext: qsTr("Clear Focus")
            onTriggered: timelineFocusSelection.clear()
        }

        XsMenuItem {
            mytext: qsTr("Move Left")
            onTriggered: {
                if(timelineSelection.selectedIndexes.length) {
                    moveItem(timelineSelection.selectedIndexes[0], -1)
                }
            }
        }
        XsMenuItem {
            mytext: qsTr("Move Right")
            onTriggered: {
                if(timelineSelection.selectedIndexes.length) {
                    moveItem(timelineSelection.selectedIndexes[0], 1)
                }
            }
        }

        XsMenuItem {
            mytext: qsTr("JumptoStart")
            onTriggered: jumpToStart()
        }

        XsMenuItem {
            mytext: qsTr("JumptoEnd")
            onTriggered: jumpToEnd()
        }

        XsMenuItem {
            mytext: qsTr("Align Left")
            onTriggered: leftAlignItems(timelineSelection.selectedIndexes)
        }
        XsMenuItem {
            mytext: qsTr("Align Right")
            onTriggered: rightAlignItems(timelineSelection.selectedIndexes)
        }
        XsMenuItem {
            mytext: qsTr("Move range")
            onTriggered: moveItemFrames(timelineSelection.selectedIndexes[0], 0, 20, 40, true)
        }
        XsMenuItem {
            mytext: qsTr("Delete")
            onTriggered: deleteItems(timelineSelection.selectedIndexes)
        }
        XsMenuItem {
            mytext: qsTr("Delete Range")
            onTriggered: deleteItemFrames(timelineSelection.selectedIndexes[0], 10, 20)
        }
        XsMenuItem {
            mytext: qsTr("Undo")
            onTriggered: undo(app_window.currentSource.index)
        }
        XsMenuItem {
            mytext: qsTr("Redo")
            onTriggered: redo(app_window.currentSource.index)
        }
        XsMenuItem {
            mytext: qsTr("Enable")
            onTriggered: enableItems(timelineSelection.selectedIndexes, true)
        }
        XsMenuItem {
            mytext: qsTr("Disable")
            onTriggered: enableItems(timelineSelection.selectedIndexes, false)
        }
        XsMenuItem {
            mytext: qsTr("Add Media")
            onTriggered: addClip(
                timelineSelection.selectedIndexes[0].parent, timelineSelection.selectedIndexes[0].row,
                app_window.mediaImageSource.index.parent
            )
        }

        XsMenuItem {
            mytext: qsTr("Add Gap")
            onTriggered: addGap(timelineSelection.selectedIndexes[0].parent, timelineSelection.selectedIndexes[0].row)
        }

        XsMenuItem {
            mytext: qsTr("Split")
            onTriggered: {
                if(timelineSelection.selectedIndexes.length) {
                    let index = timelineSelection.selectedIndexes[0]
                    splitClip(index, app_window.sessionModel.get(index, "trimmedStartRole") + (app_window.sessionModel.get(index, "trimmedDurationRole") /2))
                }
            }
        }

        XsMenuItem {
            mytext: qsTr("Duplicate")
            onTriggered: {
                let indexes = timelineSelection.selectedIndexes
                for(let i=0;i<indexes.length; i++) {
                    app_window.sessionModel.duplicateRows(indexes[i].row, 1, indexes[i].parent)
                }
            }
        }
        XsMenuItem {
            mytext: qsTr("Change Name")
            onTriggered: {
                let indexes = timelineSelection.selectedIndexes
                for(let i=0;i<indexes.length; i++) {
                    set_name_dialog.index = indexes[i]
                    set_name_dialog.text = app_window.sessionModel.get(indexes[i], "nameRole")
                    set_name_dialog.open()
                }
            }
        }
        XsMenuItem {
            mytext: qsTr("Add Item")
            onTriggered: {
                if(timelineSelection.selectedIndexes.length) {
                    new_item_dialog.insertion_parent = timelineSelection.selectedIndexes[0].parent
                    new_item_dialog.insertion_row = timelineSelection.selectedIndexes[0].row
                }
                else {
                    new_item_dialog.insertion_parent = app_window.currentSource.index
                    new_item_dialog.insertion_row = 0
                }
                new_item_dialog.open()
            }
        }
    }

    XsDragLeft {
        id: dragLeft
        visible: false
        x: 0
        y: 0
        width: 10
        height: itemHeight * scaleY
        thickness: 3
        z:10
    }

    XsDragRight {
        id: dragRight
        visible: false
        x: 0
        y: 0
        width: 10
        height: itemHeight * scaleY
        thickness: 3
        z:10
    }

    XsDragBoth {
        id: dragBothLeft
        visible: false
        x: 0
        y: 0
        width: 20
        height: itemHeight * scaleY
        thickness: 3
        z:10
    }

    XsMoveClip {
        id: moveClip
        visible: false
        x: 0
        y: 0
        width: 20
        height: itemHeight * scaleY
        thickness: 3
        z:10
    }

    XsDragBoth {
        id: dragBothRight
        visible: false
        x: 0
        y: 0
        width: 20
        height: itemHeight * scaleY
        thickness: 3
        z:10
    }

    XsDragBoth {
        id: dragAvailable
        visible: false
        x: 0
        y: 0
        width: 20
        height: 40
        thickness: 2
        z:10
    }

    XsMediaMoveCopyDialog {
        id: media_move_copy_dialog
    }

    DropArea {
        id: drop_area
        keys: [
            "text/uri-list",
            "xstudio/media-ids",
            "xstudio/timeline-ids",
            "application/x-dneg-ivy-entities-v1"
        ]
        anchors.fill: parent

        property var modelIndex: null

        onEntered: {
            processPosition(drag.x, drag.y)
        }

        onExited: {
            modelIndex = null
            dragAvailable.visible = false
            dragBothLeft.visible = false
            dragBothRight.visible = false
            dragLeft.visible = false
            dragRight.visible = false
            moveClip.visible = false
        }

        function processPosition(x,y) {
            // console.log("processPosition", resolveItem(x, y))
            let [item, item_type, local_x, local_y] = resolveItem(x, y)
            let handle = 16
            let show_dragAvailable = false
            let show_dragBothLeft = false
            let show_dragBothRight = false
            let show_dragLeft = false
            let show_dragRight = false
            let show_moveClip = false

            // update ovelay to indicate drop location.
            if(item) {
                if(["Clip","Gap"].includes(item_type)) {
                    if(local_x >= 0 && local_x < handle) {
                        let ppos = mapFromItem(item, 0, 0)
                        let item_row = item.modelIndex().row
                        if(item_row) {
                            dragBothLeft.x = ppos.x -dragBothLeft.width / 2
                            dragBothLeft.y = ppos.y
                            show_dragBothLeft = true
                        } else {
                            dragLeft.x = ppos.x
                            dragLeft.y = ppos.y
                            show_dragLeft = true
                        }
                        modelIndex = item.modelIndex()
                    }
                    else if(local_x >= item.width - handle && local_x < item.width) {
                        let ppos = mapFromItem(item, item.width, 0)
                        let item_row = item.modelIndex().row
                        if(item_row == item.modelIndex().model.rowCount(item.modelIndex().parent)-1) {
                            dragRight.x = ppos.x - dragRight.width
                            dragRight.y = ppos.y
                            show_dragRight = true
                            modelIndex = item.modelIndex().parent
                        } else {
                            dragBothRight.x = ppos.x -dragBothRight.width / 2
                            dragBothRight.y = ppos.y
                            show_dragBothRight = true
                            modelIndex = item.modelIndex().model.index(item_row+1,0,item.modelIndex().parent)
                        }
                    }
                } else if(["Audio Track","Video Track"].includes(item_type)) {
                    let ppos = mapFromItem(item, trackHeaderWidth, 0)
                    dragRight.x = ppos.x - dragRight.width
                    dragRight.y = ppos.y
                    show_dragRight = true
                    modelIndex = item.modelIndex()
                }
            }

            if(show_dragLeft != dragLeft.visible)
                dragLeft.visible = show_dragLeft

            if(show_dragRight != dragRight.visible)
                dragRight.visible = show_dragRight

            if(show_dragBothLeft != dragBothLeft.visible)
                dragBothLeft.visible = show_dragBothLeft

            if(show_dragBothRight != dragBothRight.visible)
                dragBothRight.visible = show_dragBothRight

            if(show_dragAvailable != dragAvailable.visible)
                dragAvailable.visible = show_dragAvailable

            if(show_moveClip != moveClip.visible)
                moveClip.visible = show_moveClip
        }

        onPositionChanged: {
            processPosition(drag.x, drag.y)
        }

        onDropped: {
            processPosition(drop.x, drop.y)
            if(modelIndex != null) {
                handleDrop(modelIndex, drop)
                modelIndex = null
            }
            dragAvailable.visible = false
            dragBothLeft.visible = false
            dragBothRight.visible = false
            dragLeft.visible = false
            moveClip.visible = false
            dragRight.visible = false
        }
    }

    Keys.onReleased: {
        if(event.key == Qt.Key_U && event.modifiers == Qt.ControlModifier) {
            // UNDO
            undo(app_window.currentSource.index);
            event.accepted = true
        } else if(event.key == Qt.Key_Z && event.modifiers == Qt.ControlModifier) {
            // REDO
            redo(app_window.currentSource.index);
            event.accepted = true
        }
    }


    Item {
        id: dragContainer
        anchors.fill: parent
        // anchors.topMargin: 20

        property alias dragged_items: dragged_items

        ItemSelectionModel {
            id: dragged_items
        }

        Drag.active: moveDragHandler.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction

        function startDrag(mode) {
            dragContainer.Drag.supportedActions = mode
            let indexs = timeline.timelineSelection.selectedIndexes

            dragged_items.model = timeline.timelineSelection.model
            dragged_items.select(
                helpers.createItemSelection(timeline.timelineSelection.selectedIndexes),
                ItemSelectionModel.ClearAndSelect
            )

            let ids = []

            // order by row not selection order..

            for(let i=0;i<indexs.length; ++i) {
                ids.push([indexs[i].row, indexs[i].model.get(indexs[i], "idRole")])
            }
            ids = ids.sort((a,b) => a[0] - b[0] )
            for(let i=0;i<ids.length; ++i) {
                ids[i] = ids[i][1]
            }

            if(!ids.length) {
                // cancel drag nothing being dragged.
                dragContainer.Drag.cancel()
            } else {
                dragContainer.Drag.mimeData = {
                    "xstudio/timeline-ids": ids.join("\n")
                }
            }
        }

        DragHandler {
            id: moveDragHandler
            // acceptedModifiers: Qt.NoModifier
            target: null
            onActiveChanged: {
                if(active) {
                    dragContainer.startDrag(Qt.MoveAction)
                } else{
                    // no idea why I have to do this
                    dragItem.x = 0
                    dragItem.y = 0
                    dragged_items.clear()
                }
            }
            enabled: false
        }

        Rectangle {
            id: dragItem
            color: "transparent"
            visible: false
            width: row.childrenRect.width
            height: timeline.itemHeight * timeline.scaleY

            Connections {
                target: timeline.timelineSelection
                function onSelectionChanged(selected, deselected) {
                    repeater.model = timeline.timelineSelection.selectedIndexes.length
                }
            }

            Row {
                id: row
                Repeater {
                    id: repeater
                    model: timeline.timelineSelection.selectedIndexes.length

                    XsClipItem {
                        property var itemIndex: timeline.timelineSelection.selectedIndexes[index]

                        width: duration * timeline.scaleX
                        height: timeline.itemHeight * timeline.scaleY
                        duration: timeline.timelineSelection.model.get(itemIndex, "trimmedDurationRole")
                        start: timeline.timelineSelection.model.get(itemIndex, "trimmedStartRole")
                        name: timeline.timelineSelection.model.get(itemIndex, "nameRole")
                        isEnabled: true
                    }
                }

                onPositioningComplete: {
                    if(dragItem.width)
                        dragItem.grabToImage(function(result){
                            dragContainer.Drag.imageSource = result.url
                        })
                }
            }
        }


        MouseArea{
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            propagateComposedEvents: true

            property bool isResizing: false
            property var resizeItem: null
            property var resizePreceedingItem: null
            property var resizeAnteceedingItem: null
            property string resizeItemType: ""
            property real resizeItemStartX: 0.0

            function isValidSelection(ctype, ntype) {
                let result = false
                if(["Clip","Gap"].includes(ctype) && ["Clip","Gap"].includes(ntype))
                    result = true
                else
                    result = ctype == ntype

                return result
            }

            function adjustSelection(mouse) {
                if(hovered != null) {
                    if (mouse.modifiers == Qt.ControlModifier) {
                        // validate selection, we don't allow mixed items..
                        let isValid = true
                        if(timelineSelection.selectedIndexes) {
                            let ctype = timelineSelection.selectedIndexes[0].model.get(timelineSelection.selectedIndexes[0], "typeRole")
                            isValid = isValidSelection(ctype, hovered.itemTypeRole)
                        }
                        if(isValid)
                            timelineSelection.select(hovered.modelIndex(), hovered.isSelected ? ItemSelectionModel.Deselect : ItemSelectionModel.Select)
                    } else if(mouse.modifiers == Qt.NoModifier) {
                        if(hovered.itemTypeRole == "Clip" && hovered.hasMedia) {
                            // find media in media list and select ?
                            let mind = hovered.mediaIndex
                            if(mind.valid) {
                                app_window.mediaSelectionModel.select(mind, ItemSelectionModel.ClearAndSelect)
                            }
                        }
                        timelineSelection.select(hovered.modelIndex(), ItemSelectionModel.ClearAndSelect)
                        // console.log(hovered.modelIndex())
                    }
                }
            }

            onContainsMouseChanged: {
                if(containsMouse) {
                    forceActiveFocus()
                }
            }

            onReleased: {
                if(isResizing) {
                    if(resizeItem != null) {
                        if(dragRight.visible) {
                            let mindex = resizeItem.modelIndex()
                            let src_model = mindex.model

                            src_model.set(mindex, resizeItem.durationFrame, "activeDurationRole")
                            resizeItem.isAdjustingDuration = false

                            if(resizeAnteceedingItem) {
                                if(resizeAnteceedingItem.durationFrame == 0) {
                                    app_window.sessionModel.removeTimelineItems([resizeAnteceedingItem.modelIndex()])
                                    resizeAnteceedingItem = null
                                } else {
                                    src_model.set(resizeAnteceedingItem.modelIndex(), resizeAnteceedingItem.durationFrame, "activeDurationRole")
                                    src_model.set(resizeAnteceedingItem.modelIndex(), resizeAnteceedingItem.durationFrame, "availableDurationRole")
                                    resizeAnteceedingItem.isAdjustingDuration = false
                                }
                            } else {
                                if(resizeItem.adjustAnteceedingGap > 0) {
                                    app_window.sessionModel.insertTimelineGap(mindex.row+1, mindex.parent, resizeItem.adjustAnteceedingGap, resizeItem.fps, "New Gap")
                                }
                                resizeItem.adjustAnteceedingGap = 0
                            }

                        } else if(dragLeft.visible) {
                            let mindex = resizeItem.modelIndex()
                            let src_model = mindex.model
                            src_model.set(mindex, resizeItem.startFrame, "activeStartRole")
                            src_model.set(mindex, resizeItem.durationFrame, "activeDurationRole")
                            resizeItem.isAdjustingStart = false
                            resizeItem.isAdjustingDuration = false

                            if(resizePreceedingItem) {
                                if(resizePreceedingItem.durationFrame == 0) {
                                    app_window.sessionModel.removeTimelineItems([resizePreceedingItem.modelIndex()])
                                    resizePreceedingItem = null
                                } else {
                                    src_model.set(resizePreceedingItem.modelIndex(), resizePreceedingItem.durationFrame, "activeDurationRole")
                                    src_model.set(resizePreceedingItem.modelIndex(), resizePreceedingItem.durationFrame, "availableDurationRole")
                                    resizePreceedingItem.isAdjustingDuration = false
                                }
                            } else {
                                if(resizeItem.adjustPreceedingGap > 0) {
                                    app_window.sessionModel.insertTimelineGap(mindex.row, mindex.parent, resizeItem.adjustPreceedingGap, resizeItem.fps, "New Gap")
                                }
                                resizeItem.adjustPreceedingGap = 0
                            }
                        } else if(dragAvailable.visible) {
                            let src_model = resizeItem.modelIndex().model
                            src_model.set(resizeItem.modelIndex(), resizeItem.startFrame, "activeStartRole")
                            resizeItem.isAdjustingStart = false
                        } else if(dragBothLeft.visible) {
                            let mindex = resizeItem.modelIndex()
                            let src_model = mindex.model
                            src_model.set(mindex, resizeItem.startFrame, "activeStartRole")
                            src_model.set(mindex, resizeItem.durationFrame, "activeDurationRole")

                            if(resizePreceedingItem) {
                                let pindex = src_model.index(mindex.row-1, 0, mindex.parent)
                                src_model.set(pindex, resizePreceedingItem.durationFrame, "activeDurationRole")
                            }
                            resizeItem.isAdjustingStart = false
                            resizeItem.isAdjustingDuration = false
                        } else if(dragBothRight.visible) {
                            let mindex = resizeItem.modelIndex()
                            let src_model = mindex.model
                            src_model.set(mindex, resizeItem.durationFrame, "activeDurationRole")

                            let pindex = src_model.index(mindex.row + 1, 0, mindex.parent)
                            src_model.set(pindex, resizeAnteceedingItem.startFrame, "activeStartRole")
                            src_model.set(pindex, resizeAnteceedingItem.durationFrame, "activeDurationRole")

                            resizeItem.isAdjustingDuration = false
                        } else if(moveClip.visible) {
                            let mindex = resizeItem.modelIndex()
                            let src_model = mindex.model

                            if(resizePreceedingItem && resizePreceedingItem.durationFrame) {
                                src_model.set(resizePreceedingItem.modelIndex(), resizePreceedingItem.durationFrame, "activeDurationRole")
                                src_model.set(resizePreceedingItem.modelIndex(), resizePreceedingItem.durationFrame, "availableDurationRole")
                            }

                            if(resizeAnteceedingItem && resizeAnteceedingItem.durationFrame) {
                                src_model.set(resizeAnteceedingItem.modelIndex(), resizeAnteceedingItem.durationFrame, "activeDurationRole")
                                src_model.set(resizeAnteceedingItem.modelIndex(), resizeAnteceedingItem.durationFrame, "availableDurationRole")
                            }

                            let delete_preceeding = resizePreceedingItem && !resizePreceedingItem.durationFrame
                            let delete_anteceeding = resizeAnteceedingItem && !resizeAnteceedingItem.durationFrame
                            let insert_preceeding = resizeItem.isAdjustPreceeding && resizeItem.adjustPreceedingGap
                            let insert_anteceeding = resizeItem.isAdjustAnteceeding && resizeItem.adjustAnteceedingGap

                            // some operations are moves
                            if(insert_preceeding && delete_anteceeding) {
                                // move clip left
                                moveItem(resizeItem.modelIndex(), 1)
                            } else if (delete_preceeding && insert_anteceeding) {
                                moveItem(resizeItem.modelIndex(), -1)
                            } else {
                                if(delete_preceeding) {
                                    app_window.sessionModel.removeTimelineItems([resizePreceedingItem.modelIndex()])
                                }

                                if(delete_anteceeding) {
                                    app_window.sessionModel.removeTimelineItems([resizeAnteceedingItem.modelIndex()])
                                }

                                if(insert_preceeding) {
                                    app_window.sessionModel.insertTimelineGap(mindex.row, mindex.parent, resizeItem.adjustPreceedingGap, resizeItem.fps, "New Gap")
                                }

                                if(insert_anteceeding) {
                                    app_window.sessionModel.insertTimelineGap(mindex.row + 1, mindex.parent, resizeItem.adjustAnteceedingGap, resizeItem.fps, "New Gap")
                                }
                            }

                            resizeItem.adjustPreceedingGap = 0
                            resizeItem.isAdjustPreceeding = false
                            resizeItem.adjustAnteceedingGap = 0
                            resizeItem.isAdjustAnteceeding = false

                        }

                        if(resizePreceedingItem) {
                            resizePreceedingItem.isAdjustingStart = false
                            resizePreceedingItem.isAdjustingDuration = false
                        }

                        if(resizeAnteceedingItem) {
                            resizeAnteceedingItem.isAdjustingStart = false
                            resizeAnteceedingItem.isAdjustingDuration = false
                        }

                        resizeItem = null
                    }

                    resizeAnteceedingItem = null
                    resizePreceedingItem = null
                    isResizing = false
                    dragLeft.visible = false
                    dragRight.visible = false
                    dragBothLeft.visible = false
                    moveClip.visible = false
                    dragBothRight.visible = false
                    dragAvailable.visible = false
                } else {
                    moveDragHandler.enabled = false
                }
            }

            onPressed: {
                if(mouse.button == Qt.RightButton) {
                    adjustSelection(mouse)
                    timelineMenu.popup()
                } else if(mouse.button == Qt.LeftButton) {
                    adjustSelection(mouse)
                }

                if(dragLeft.visible || dragRight.visible || dragBothLeft.visible || dragBothRight.visible || dragAvailable.visible || moveClip.visible) {
                    let [item, item_type, local_x, local_y] = resolveItem(mouse.x, mouse.y)
                    resizeItem = item
                    resizeItemStartX = mouse.x
                    resizeItemType = item_type
                    isResizing = true
                    if(dragLeft.visible) {
                        resizeItem.adjustDuration = 0
                        resizeItem.adjustStart = 0
                        resizeItem.isAdjustingDuration = true
                        resizeItem.isAdjustingStart = true
                        // is there a gap to our left..
                        let mi = resizeItem.modelIndex()
                        let pre_index = preceedingIndex(mi)
                        if(pre_index.valid) {
                            let preceeding_type = pre_index.model.get(pre_index, "typeRole")

                            if(preceeding_type == "Gap") {
                                resizePreceedingItem = resizeItem.parentLV.itemAtIndex(mi.row - 1)
                                resizePreceedingItem.adjustDuration = 0
                                resizePreceedingItem.isAdjustingDuration = true
                            }
                        }
                    } else if(dragRight.visible) {
                        resizeItem.adjustDuration = 0
                        resizeItem.isAdjustingDuration = true

                        let mi = resizeItem.modelIndex()
                        let ante_index = anteceedingIndex(mi)
                        if(ante_index.valid) {
                            let anteceeding_type = ante_index.model.get(ante_index, "typeRole")

                            if(anteceeding_type == "Gap") {
                                resizeAnteceedingItem = resizeItem.parentLV.itemAtIndex(mi.row + 1)
                                resizeAnteceedingItem.adjustDuration = 0
                                resizeAnteceedingItem.isAdjustingDuration = true
                            }
                        }
                    } else if(dragAvailable.visible) {
                        resizeItem.adjustStart = 0
                        resizeItem.isAdjustingStart = true
                    } else if(dragBothLeft.visible) {
                        // both at front or end..?
                        let mi = resizeItem.modelIndex()
                        resizeItem.adjustDuration = 0
                        resizeItem.adjustStart = 0
                        resizeItem.isAdjustingStart = true
                        resizeItem.isAdjustingDuration = true

                        resizePreceedingItem = resizeItem.parentLV.itemAtIndex(mi.row - 1)
                        resizePreceedingItem.adjustDuration = 0
                        resizePreceedingItem.isAdjustingDuration = true
                    } else if(dragBothRight.visible) {
                        // both at front or end..?
                        let mi = resizeItem.modelIndex()
                        resizeItem.adjustDuration = 0
                        resizeItem.isAdjustingDuration = true

                        resizeAnteceedingItem = resizeItem.parentLV.itemAtIndex(mi.row + 1)
                        resizeAnteceedingItem.adjustStart = 0
                        resizeAnteceedingItem.adjustDuration = 0
                        resizeAnteceedingItem.isAdjustingStart = true
                        resizeAnteceedingItem.isAdjustingDuration = true
                    } else if(moveClip.visible) {
                        // we adjust material either side of us..
                        let mi = resizeItem.modelIndex()
                        let prec_index = preceedingIndex(mi)
                        let ante_index = anteceedingIndex(mi)

                        let preceeding_type = prec_index.valid ? prec_index.model.get(prec_index, "typeRole") : "Track"
                        let anteceeding_type = ante_index.valid ? ante_index.model.get(ante_index, "typeRole") : "Track"

                        if(preceeding_type == "Gap") {
                            resizePreceedingItem = resizeItem.parentLV.itemAtIndex(mi.row - 1)
                            resizePreceedingItem.adjustDuration = 0
                            resizePreceedingItem.isAdjustingDuration = true
                        } else {
                            resizeItem.adjustPreceedingGap = 0
                            resizeItem.isAdjustPreceeding = true
                        }

                        if(anteceeding_type == "Gap") {
                            resizeAnteceedingItem = resizeItem.parentLV.itemAtIndex(mi.row + 1)
                            resizeAnteceedingItem.adjustDuration = 0
                            resizeAnteceedingItem.isAdjustingDuration = true
                        } else if(anteceeding_type != "Track") {
                            resizeItem.adjustAnteceedingGap = 0
                            resizeItem.isAdjustAnteceeding = true
                        }
                    }
                } else {
                    let [item, item_type, local_x, local_y] = resolveItem(mouse.x, mouse.y)
                    if(item_type != null && item_type != "Stack" && timelineSelection.isSelected(item.modelIndex())) {
                        moveDragHandler.enabled = true
                    }
                }
            }

            onPositionChanged: {
                if(isResizing) {
                    let frame_change = -((resizeItemStartX - mouse.x) / scaleX)

                    if(dragRight.visible) {

                        frame_change = resizeItem.checkAdjust(frame_change, true)
                        if(resizeAnteceedingItem) {
                            frame_change = -resizeAnteceedingItem.checkAdjust(-frame_change, false)
                            resizeAnteceedingItem.adjust(-frame_change)
                        } else {
                            resizeItem.adjustAnteceedingGap = -frame_change
                        }

                        resizeItem.adjust(frame_change)

                        let ppos = mapFromItem(resizeItem, resizeItem.width - (resizeItem.adjustAnteceedingGap * scaleX) - dragRight.width, 0)
                        dragRight.x = ppos.x
                    } else if(dragLeft.visible) {
                        // must inject / resize gap.
                        // make sure last frame doesn't change..
                        frame_change = resizeItem.checkAdjust(frame_change, false, true)
                        if(resizePreceedingItem) {
                            frame_change = resizePreceedingItem.checkAdjust(frame_change, false)
                            resizePreceedingItem.adjust(frame_change)
                        } else {
                            resizeItem.adjustPreceedingGap = frame_change
                        }

                        resizeItem.adjust(frame_change)

                        let ppos = mapFromItem(resizeItem, resizeItem.adjustPreceedingGap * scaleX, 0)
                        dragLeft.x = ppos.x
                    } else if(dragBothLeft.visible) {
                        frame_change = resizeItem.checkAdjust(frame_change, true)
                        frame_change = resizePreceedingItem.checkAdjust(frame_change, true)

                        resizeItem.adjust(frame_change)
                        resizePreceedingItem.adjust(frame_change)

                        let ppos = mapFromItem(resizeItem, -dragBothLeft.width / 2, 0)
                        dragBothLeft.x = ppos.x
                    } else if(dragBothRight.visible) {
                        frame_change = resizeItem.checkAdjust(frame_change, true)
                        frame_change = resizeAnteceedingItem.checkAdjust(frame_change, true)

                        resizeItem.adjust(frame_change)
                        resizeAnteceedingItem.adjust(frame_change)

                        let ppos = mapFromItem(resizeItem, resizeItem.width - dragBothRight.width / 2, 0)
                        dragBothRight.x = ppos.x
                    } else if(dragAvailable.visible) {
                        resizeItem.updateStart(resizeItemStartX, mouse.x)
                    } else if(moveClip.visible) {
                        if(resizePreceedingItem)
                            frame_change = resizePreceedingItem.checkAdjust(frame_change, false)
                        else
                            frame_change = Math.max(0, frame_change)

                        if(resizeAnteceedingItem)
                            frame_change = -resizeAnteceedingItem.checkAdjust(-frame_change, false)
                        // else
                        //     frame_change = Math.max(0, frame_change)

                        if(resizePreceedingItem)
                            resizePreceedingItem.adjust(frame_change)
                        else if(resizeItem.isAdjustPreceeding)
                            resizeItem.adjustPreceedingGap = frame_change

                        if(resizeAnteceedingItem)
                            resizeAnteceedingItem.adjust(-frame_change)
                        else if(resizeItem.isAdjustAnteceeding)
                            resizeItem.adjustAnteceedingGap = -frame_change

                        let ppos = mapFromItem(resizeItem, resizeItem.width / 2 - moveClip.width / 2, 0)
                        moveClip.x = ppos.x
                    }
                } else {
                    let [item, item_type, local_x, local_y] = resolveItem(mouse.x, mouse.y)

                    if(hovered != item) {
                        // console.log(item,item.modelIndex(), item_type, local_x, local_y)
                        hovered = item
                    }

                    let show_dragLeft = false
                    let show_dragRight = false
                    let show_dragBothLeft = false
                    let show_moveClip = false
                    let show_dragBothRight = false
                    let show_dragAvailable = false
                    let handle = 32

                    if(hovered) {
                        if("Clip" == item_type) {

                            let preceeding_type = "Track"
                            let anteceeding_type = "Track"

                            let mi = item.modelIndex()

                            let ante_index = anteceedingIndex(mi)
                            let pre_index = preceedingIndex(mi)

                            if(ante_index.valid)
                                anteceeding_type = ante_index.model.get(ante_index, "typeRole")

                            if(pre_index.valid)
                                preceeding_type = pre_index.model.get(pre_index, "typeRole")

                            // expand left
                            let left = local_x <= (handle * 1.5) && local_x >= 0
                            let left_edge = left && local_x < (handle / 2)
                            let right = local_x >= hovered.width - (1.5 * handle) && local_x < hovered.width
                            let right_edge = right && local_x > hovered.width - (handle / 2)
                            let middle = local_x >= (hovered.width/2) - (handle / 2) && local_x <= (hovered.width/2) + (handle / 2)

                            if(preceeding_type == "Clip" && left_edge)  {
                                let ppos = mapFromItem(item, -dragBothLeft.width / 2, 0)
                                dragBothLeft.x = ppos.x
                                dragBothLeft.y = ppos.y
                                show_dragBothLeft = true
                                item.parentLV.itemAtIndex(mi.row - 1).isBothHovered = true
                            } else if(left) {
                                let ppos = mapFromItem(item, 0, 0)
                                dragLeft.x = ppos.x
                                dragLeft.y = ppos.y
                                show_dragLeft = true
                                if(preceeding_type == "Clip")
                                    item.parentLV.itemAtIndex(mi.row - 1).isBothHovered = false
                            } else if(anteceeding_type == "Clip" && right_edge) {
                                let ppos = mapFromItem(item, hovered.width - dragBothRight.width/2, 0)
                                dragBothRight.x = ppos.x
                                dragBothRight.y = ppos.y
                                show_dragBothRight = true
                                item.parentLV.itemAtIndex(mi.row + 1).isBothHovered = true
                            } else if(right) {
                                let ppos = mapFromItem(item, hovered.width - dragRight.width, 0)
                                dragRight.x = ppos.x
                                dragRight.y = ppos.y
                                show_dragRight = true
                                if(anteceeding_type == "Clip")
                                    item.parentLV.itemAtIndex(mi.row + 1).isBothHovered = false
                            } else if(middle && (preceeding_type != "Clip" || anteceeding_type != "Clip") && !(preceeding_type == "Track" && anteceeding_type == "Clip")) {
                                let ppos = mapFromItem(item, hovered.width / 2, hovered.height / 2)
                                moveClip.x = ppos.x - moveClip.width / 2
                                moveClip.y = ppos.y - moveClip.height / 2
                                show_moveClip = true
                            } else if("Clip" == item_type && local_y >= 0 && local_y <= 8) {
                                // available range..
                                let ppos = mapFromItem(item, hovered.width / 2, 0)
                                dragAvailable.x = ppos.x -dragAvailable.width / 2
                                dragAvailable.y = ppos.y - dragAvailable.height / 2
                                show_dragAvailable = true
                            }
                        }
                     }

                    if(show_dragLeft != dragLeft.visible)
                        dragLeft.visible = show_dragLeft

                    if(show_dragRight != dragRight.visible)
                        dragRight.visible = show_dragRight

                    if(show_dragBothLeft != dragBothLeft.visible)
                        dragBothLeft.visible = show_dragBothLeft

                    if(show_moveClip != moveClip.visible)
                        moveClip.visible = show_moveClip

                    if(show_dragBothRight != dragBothRight.visible)
                        dragBothRight.visible = show_dragBothRight

                    if(show_dragAvailable != dragAvailable.visible)
                        dragAvailable.visible = show_dragAvailable
                }
            }

            onWheel: {
                // maintain position as we zoom..
                if(wheel.modifiers == Qt.ShiftModifier) {
                    if(wheel.angleDelta.y > 1) {
                        scaleX += 0.2
                        scaleY += 0.2
                    } else {
                        scaleX -= 0.2
                        scaleY -= 0.2
                    }
                    wheel.accepted = true
                    // console.log(wheel.x, wheel.y)
                } else if(wheel.modifiers == Qt.ControlModifier) {
                    if(wheel.angleDelta.y > 1) {
                        scaleX += 0.2
                    } else {
                        scaleX -= 0.2
                    }
                    wheel.accepted = true
                } else if(wheel.modifiers == (Qt.ControlModifier | Qt.ShiftModifier)) {
                    if(wheel.angleDelta.y > 1) {
                        scaleY += 0.2
                    } else {
                        scaleY -= 0.2
                    }
                    wheel.accepted = true
                } else {
                    wheel.accepted = false
                }


                if(wheel.accepted) {
                    list_view.itemAtIndex(0).jumpToFrame(viewport.playhead.frame, ListView.Center)
                    // let current_frame = list_view.itemAtIndex(0).currentFrame()
                    // jumpToFrame(viewport.playhead.frame, false)
                }
            }

            Connections {
                target: timeline
                function onJumpToStart() {
                    list_view.itemAtIndex(0).jumpToStart()
                }
                function onJumpToEnd() {
                    list_view.itemAtIndex(0).jumpToEnd()
                }
            }

            ListView {
                anchors.fill: parent
                interactive: false
                id:list_view
                model: timeline_items
                orientation: ListView.Horizontal

                property var timelineItem: timeline
                property var hoveredItem: hovered
                property real scaleX: timeline.scaleX
                property real scaleY: timeline.scaleY
                property real itemHeight: timeline.itemHeight
                property real trackHeaderWidth: timeline.trackHeaderWidth
                property var setTrackHeaderWidth: timeline.setTrackHeaderWidth
                property var timelineSelection: timeline.timelineSelection
                property var timelineFocusSelection: timeline.timelineFocusSelection
                property int playheadFrame: viewport.playhead.frame
                property string itemFlag: ""

                onPlayheadFrameChanged: {
                    if (itemAtIndex(0)) {
                        itemAtIndex(0).jumpToFrame(playheadFrame, ListView.Visible)
                    }
                }
            }
        }
    }
}