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

import xStudio 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

Rectangle {

    id: timeline

    color: timelineBackground

    property var hovered: null
    property real scaleX: 3.0
    property real scaleY: verticalScale
    property real itemHeight: 30.0
    property real trackHeaderWidth: 250.0

    property color timelineBackground: XsStyleSheet.panelBgColor //palette.panelBgColor
    property color timelineText: Qt.darker("#AFAFAF", XsStyleSheet.textDarkerFactor)
    property color trackBackground:  Qt.darker("#474747", XsStyleSheet.darkerFactor)
    property color trackEdge: Qt.darker("#3B3B3B", XsStyleSheet.darkerFactor)
    property color defaultClip: Qt.darker("#888888", XsStyleSheet.darkerFactor)

    property alias timelineSelection: timelineSelection

    property alias select_up_hotkey: select_up_hotkey
    property alias select_down_hotkey: select_down_hotkey
    property alias expand_up_hotkey: expand_up_hotkey
    property alias expand_down_hotkey: expand_down_hotkey
    property alias contract_up_hotkey: contract_up_hotkey
    property alias contract_down_hotkey: contract_down_hotkey
    property alias expand_up_down_hotkey: expand_up_down_hotkey
    property alias contract_up_down_hotkey: contract_up_down_hotkey

    property alias select_next_hotkey: select_next_hotkey
    property alias select_previous_hotkey: select_previous_hotkey
    property alias expand_next_hotkey: expand_next_hotkey
    property alias expand_previous_hotkey: expand_previous_hotkey
    property alias expand_both_hotkey: expand_both_hotkey
    property alias contract_next_hotkey: contract_next_hotkey
    property alias contract_previous_hotkey: contract_previous_hotkey
    property alias contract_both_hotkey: contract_both_hotkey

    property bool selectionIsLocked: false
    property bool selectionIsEnabled: false
    property bool loopSelection: false
    property bool focusSelection: false
    property alias timeline_items: list_view.model
    property alias timelineModel: list_view.model

    property alias timelineMarkerMenu: markerMenu
    property alias timelineProperty: timelineProperty
    property string editMode: "Select"
    property bool rippleMode: false
    property bool overwriteMode: false
    property bool snapMode: false

    property bool scalingModeActive: false
    property bool scrollingModeActive: false
    property var conformSourceIndex: null
    property bool have_timeline: true

    property int snapLine: -1
    property int cutLine: -1

    property var snapCacheKey: null


    signal jumpToStart()
    signal jumpToEnd()
    signal jumpToFrame(int frame, bool center)

    onScaleYChanged: verticalScale = scaleY

    onScalingModeActiveChanged: {
        if(scalingModeActive) {
            scrollingModeActive = false
            helpers.setOverrideCursor("://cursors/magnifier_cursor.svg")
        } else {
            helpers.restoreOverrideCursor()
        }
    }

    onScrollingModeActiveChanged: {
        if(scrollingModeActive) {
            scalingModeActive = false
            helpers.setOverrideCursor("Qt.OpenHandCursor")
        } else {
            helpers.restoreOverrideCursor()
        }
    }

    XsModelProperty {
        // XsModelProperty is able to 'watch' an index and emits onIndexChanged
        // when the index goes invalid. We use this to watch whether the active
        // timeline is deleted - if so we run viewedMediaSetChanged which will
        // clear our model and index because otherwise Qt crashes as it seems
        // to hang on to items that have been deleted from the model.
        id: timelineProperty
        index: timeline_items.rootIndex
        role: "propertyRole"
        onValueChanged: {updateConformSourceIndex();viewedMediaSetChanged()}
        onIndexChanged: {
            if(!index || !index.valid) {
                // perhaps the timeline we were viewing has been deleted?
                viewedMediaSetChanged()
            }
        }
    }

    function updateConformSourceIndex() {
        // resolve to track index.
        let m = timeline_items.srcModel
        if(m == theSessionData) {
            let current = m.get(timeline_items.rootIndex, "propertyRole")
            let tindex = getVideoTrackIndex(1)

            if(current != undefined) {
                let stack = m.index(0, 0, timeline_items.rootIndex)
                let ncsi = m.search(helpers.QVariantFromUuidString(current.conform_track_uuid), "idRole", stack, 0)
                if(ncsi.valid) {
                    tindex = helpers.makePersistent(ncsi)
                }
            }

            conformSourceIndex = tindex
        }
    }

    onConformSourceIndexChanged: {
        if(conformSourceIndex && conformSourceIndex.valid) {
            let m = timeline_items.srcModel
            if(m == theSessionData) {
                let current = m.get(timeline_items.rootIndex, "propertyRole")
                let tid = m.get(conformSourceIndex, "idRole")

                if(current == undefined || current.conform_track_uuid != helpers.QUuidToQString(tid)) {
                    if(current == undefined) {
                        current = {"conform_track_uuid": helpers.QUuidToQString(tid)}
                    } else {
                        current.conform_track_uuid = helpers.QUuidToQString(tid)
                    }

                    m.set(timeline_items.rootIndex, current, "propertyRole")
                }
            }
        }
    }

    onLoopSelectionChanged: {
        if(loopSelection)
            updateLoop()
        else
            timelinePlayhead.enableLoopRange = false
    }

    onFocusSelectionChanged: {
        if(focusSelection)
            updateFocus()
        else
            focusItems([])
    }

    ListModel {
        id: null_list
    }

    function updateLoop() {
        if(loopSelection) {
            // get min/max timepoints.
            if(timelineSelection.selectedIndexes.length) {
                let minFrame = null
                let maxFrame = null
                timelinePlayhead.enableLoopRange = true
                for(let i=0;i<timelineSelection.selectedIndexes.length; ++i) {
                    [minFrame, maxFrame] = findItemBounds(timelineSelection.selectedIndexes[i], minFrame, maxFrame)
                }
                timelinePlayhead.loopStartFrame = minFrame
                timelinePlayhead.loopEndFrame = maxFrame
            } else {
                timelinePlayhead.enableLoopRange = false
            }
        }
    }

    function findItemBounds(index, min, max) {
        let m = index.model
        let type = m.get(index, "typeRole")
        if(type == "Clip") {
            let imin = m.startFrameInParent(index)
            let imax = imin + m.get(index, "trimmedDurationRole")
            if(min == null || min > imin)
                min = imin
            if(max == null || max < imax)
                max = imax
        } else if(type == "Audio Track" || type == "Video Track") {
            for(let i=0;i<m.rowCount(index); ++i) {
                [min, max] = findItemBounds(m.index(i,0,index), min, max)
            }
        }

        return [min, max]

    }

    function getMediaIndex(clipIndex) {
        let tindex = theSessionData.getTimelineIndex(clipIndex)
        let muuid = theSessionData.get(clipIndex, "clipMediaUuidRole")
        let mlist = theSessionData.index(0, 0, tindex)
        let result = theSessionData.search(muuid, "actorUuidRole", mlist)
        return result
    }

    function updateFocus() {
        if(focusSelection) {
            focusItems(timelineSelection.selectedIndexes)
        }
    }

    ItemSelectionModel {
        id: timelineSelection
        model: theSessionData
        onSelectionChanged: {
            updateFocus()
            updateLoop()
        }
    }

    function viewedMediaSetChanged() {

        if(viewedMediaSetProperties.index.valid && viewedMediaSetProperties.values.typeRole == "Timeline") {
            forceActiveFocus()
            let timelineIndex = theSessionData.index(2, 0, viewedMediaSetProperties.index)
            if (timelineIndex == timeline_items.rootIndex) return

            if (theSessionData.rowCount(timelineIndex) == 0) {
                // the timeline item data hasn't been 'fetched' from the backend.
                // We can force this to happen, but it will be asynchronous so
                // we need a small delay to allow it to complete before we set
                // the index on timeline_items (which triggers the build of
                // the timeline UI components)
                timeline_items.srcModel = null_list
                theSessionData.fetchMore(timelineIndex)
                callbackTimer.setTimeout(function(index) { return function() {
                    timeline_items.srcModel = theSessionData
                    timeline_items.rootIndex = helpers.makePersistent(index)
                    have_timeline = true
                    updateConformSourceIndex()
                    fitItems()
                    // conformSourceIndex = getVideoTrackIndex(1)
                    }}( timelineIndex ), 200);
            } else {
                timeline_items.srcModel = theSessionData
                timeline_items.rootIndex = helpers.makePersistent(timelineIndex)
                have_timeline = true
                updateConformSourceIndex()
                callbackTimer.setTimeout(function() { return function() {
                    fitItems()
                    }}(), 200);
            }

        } else if (!timeline_items.rootIndex.valid) {
            // if the user has selected something that is not a timeline (playlist,
            // subset etc.), we do  not update our index here (unless the timeline
            // has been deleted or something). So the NLE continues to show the last
            // timeline that we interacted with. This is so the user can hop
            // bewtween individual media and the playlist without the playlist
            // interface clearing
            timeline_items.srcModel = []
            updateConformSourceIndex()
            have_timeline = false
        }
    }

    Component.onCompleted: {
        viewedMediaSetChanged()
        //clipMenu.debugSetMenuPathPosition("Debug", 20.0)
        //trackMenu.debugSetMenuPathPosition("Debug", 20.0)
    }

    Connections {
      target: viewedMediaSetProperties
      function onIndexChanged() {
        viewedMediaSetChanged()
      }
    }

    Connections {
        target: theSessionData
        function onMakeTimelineSelection(timeline_index, timeline_item_indeces) {
            // this connection lets items remote from the Timeline UI (like the
            // Notes panel) control the selection of items in the Timeline UI.
            if (timeline_index == timeline_items.rootIndex.parent) {
                focusSelection = true
                timelineSelection.select(
                    helpers.createItemSelection(timeline_item_indeces),
                    ItemSelectionModel.ClearAndSelect
                    )
            }
        }
    }

    /*
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
    }*/

    function setTrackHeaderWidth(val) {
        trackHeaderWidth = Math.max(val, 40)
    }

    function addTrack(type, sync=true, name=null) {
        let m = timeline_items.srcModel
        let stack_index = m.index(0, 0, timeline_items.rootIndex)
        if(type == "Video Track")
            return addItem(type, stack_index, 0, name || "Video Track", sync)
        else if(type == "Audio Track")
            return addItem(type, stack_index, m.rowCount(stack_index), name || "Audio Track", sync)

        return undefined
    }

    function addItem(type, insertion_parent, insertion_row, name="New Item", sync=true) {

        // insertion type
        let insertion_index_type = theSessionData.get(insertion_parent, "typeRole")
        if(type == "Video Track") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = theSessionData.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = theSessionData.index(0, 0, insertion_parent) // stack
                insertion_row = 0
            } else if(insertion_index_type != "Stack") {
                insertion_parent = null
            }
        }
        else if(type == "Audio Track") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = theSessionData.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = theSessionData.index(0, 0, insertion_parent) // stack
                insertion_row = theSessionData.rowCount(insertion_parent) // last track + 1
            } else if(insertion_index_type != "Stack") {
                insertion_parent = null
            }
        }
        else if(type == "Gap" || type == "Clip") {
            if(insertion_index_type == "Timeline") {
                insertion_parent = theSessionData.index(2, 0, insertion_parent) // timelineitem
                insertion_parent = theSessionData.index(0, 0, insertion_parent) // stack
                insertion_parent = theSessionData.index(0, 0, insertion_parent) // track
                insertion_row = theSessionData.rowCount(insertion_parent) // last clip
            } else if (insertion_index_type == "Stack") {
                insertion_parent = theSessionData.index(insertion_row, 0, insertion_parent)
                insertion_row = theSessionData.rowCount(insertion_parent)
            } else {
                console.log(insertion_parent, insertion_index_type)
            }
        }

        if(insertion_parent != null) {
            if(sync)
                return theSessionData.insertRowsSync(insertion_row, 1, type, name, insertion_parent)
            else
                return theSessionData.insertRowsAsync(insertion_row, 1, type, name, insertion_parent)
        }

        return undefined
    }

    /*Connections {
        target: app_window
        function onFlagSelectedItems(flag) {
            if(timeline.visible) {
                let indexes = timelineSelection.selectedIndexes
                for(let i=0;i<indexes.length; i++) {
                    theSessionData.set(indexes[i], flag, "flagColourRole")
                }
            }
        }
    }*/


    // This menu is built from a menu model that is maintained by xSTUDIO's
    // backend. We access the menu model by an id string 'menuModelName' that
    // will be set by the derived type
    XsTimelineMenu {
        id: timelineMenu
        Component.onCompleted: {
            helpers.setMenuPathPosition("Debug", "timeline_menu_", 100)
        }
    }

    XsMarkerMenu {
        id: markerMenu
    }

    XsTimelineClipMenu {
        id: clipMenu
    }

    XsTimelineTrackMenu {
        id: trackMenu
    }

    XsPopupMenu {
        id: flagMenu
        visible: false
        menu_model_name: "timeline_flag_menu_"+timeline
        property var flagCallback: null
        property var panelContext: helpers.contextPanel(flagMenu)

        XsFlagMenuInserter {
            text: ""
            menuPath: ""
            panelContext: flagMenu.panelContext
            menuModelName: flagMenu.menu_model_name
            onFlagSet: {
                if(flagMenu.flagCallback)
                    flagMenu.flagCallback(flag, flag_text)
            }
        }
    }

    function showFlagMenu(mx, my, source=this, callback=null) {
        let sp = mapFromItem(source, mx, my)
        flagMenu.x = sp.x
        flagMenu.y = sp.y
        flagMenu.flagCallback = callback
        flagMenu.visible = true
    }

    function showTimelineMenu(mx, my, source=this) {
        let sp = mapFromItem(source, mx, my)
        timelineMenu.x = sp.x
        timelineMenu.y = sp.y
        timelineMenu.visible = true
    }

    function showMarkerMenu(mx, my, markerIndex, source=this) {
        let sp = mapFromItem(source, mx, my)
        markerMenu.x = sp.x
        markerMenu.y = sp.y
        markerMenu.markerIndex = markerIndex
        markerMenu.visible = true
    }

    function showClipMenu(mx, my, source=this) {
        let sp = mapFromItem(source, mx, my)
        clipMenu.x = sp.x
        clipMenu.y = sp.y
        clipMenu.visible = true
    }

    function showTrackMenu(mx, my, source=this) {
        let sp = mapFromItem(source, mx, my)
        trackMenu.x = sp.x
        trackMenu.y = sp.y
        trackMenu.visible = true
    }

    function addGap(parent, row, name = "NewGap", frames=24, rate=24.0) {
        return theSessionData.insertTimelineGap(row, parent, frames, rate, name)
    }

    function addClip(parent, row, media_index,  name = "New Clip") {
        return theSessionData.insertTimelineClip(row, parent, media_index, name)
    }

    function deleteItems(indexes) {
        theSessionData.removeTimelineItems(indexes, rippleMode);
    }

    function flagItems(indexes, flag) {
        for(let i=0; i< indexes.length; i++) {
            indexes[i].model.set(indexes[i], flag, "flagColourRole")
        }
    }

    function duplicate(indexes) {
        if(indexes.length && theSessionData.get(indexes[0], "typeRole") == "Clip")
            duplicateClips(indexes)
        else
            duplicateTracks(indexes)
    }

    function insertTrackAbove(indexes) {
        let sorted = []
        for(let i=0; i<indexes.length; ++i) {
            let type = theSessionData.get(indexes[i],"typeRole")
            if(type == "Clip") {
                sorted.push(indexes[i].parent)
            } else if(["Audio Track", "Video Track"].includes(type))
                sorted.push(indexes[i])
        }

        sorted.sort((a,b) => b.row - a.row)

        for(let i=0; i< sorted.length; i++) {
            let type = theSessionData.get(sorted[i],"typeRole")
            addItem(type, sorted[i].parent, sorted[i].row + (type == "Audio Track" ? 1 : 0), type)
        }
    }

    function duplicateClips(indexes) {
        return theSessionData.duplicateTimelineClips(indexes);
    }

    function duplicateTracks(indexes) {
        for(let i=0;i<indexes.length; i++) {
            theSessionData.duplicateRows(indexes[i].row, 1, indexes[i].parent)
        }
    }

    function deleteItemFrames(index, start, duration) {
        theSessionData.removeTimelineItems(index, start, duration);
    }

    function undo(timeline_index) {
        theSessionData.undo(timeline_index)
    }

    function redo(timeline_index) {
        theSessionData.redo(timeline_index)
    }

    // we've got a bounding box for our zoom now...
    // we want to fit the window to it.. this is going to be slightly complicated..
    // as we have two virtual views insde a second one :()

    // we only manipulate the x scale and x position though, so that'll help
    // we let the user deal with Y scale / position..
    function fitItems(indexes=[]) {
        if(!indexes.length)
            indexes = [timeline_items.rootIndex]
        let r = theSessionData.timelineRect(indexes)
        let tr = theSessionData.timelineRect([timeline_items.rootIndex])
        // cap width to timeline
        let bwidth = Math.min(r.width * 1.2, tr.width)
        scaleX = (list_view.width - trackHeaderWidth)/ bwidth

        // Push middle left if we run out of clip
        let middle = r.left + (r.width/2)

        // console.log(scaleX, r.left , middle, r == tr)
        if(list_view.itemAtIndex(0))
            list_view.itemAtIndex(0).jumpToFrame(r == tr ? r.left : middle, r == tr ? ListView.Beginning : ListView.Center)
    }

    function markerModel() {
        if(list_view.itemAtIndex(0)) {
            return list_view.itemAtIndex(0).markerModel
        }
        return null
    }

    function moveItem(index, distance) {
        theSessionData.moveTimelineItem(index, distance)
    }

    // currently video track 2, one up from bottom
    function getVideoTrackIndex(ind) {
        let m = timeline_items.srcModel
        let stack_index = m.index(0, 0, timeline_items.rootIndex)
        let bottom_v = 0;

        for(let i = 0;i<m.rowCount(stack_index);i++){
            if(m.get(m.index(i, 0, stack_index), "typeRole") == "Video Track")
                bottom_v = i
        }

        return helpers.makePersistent(m.index(bottom_v-ind, 0, stack_index))
    }


    function moveItems(indexes, distance) {
        let sorted = []
        for(let i=0; i<indexes.length; ++i)
            sorted[i] = indexes[i]

        if(distance >0)
            sorted.sort((a,b) => b.row - a.row)
        else
            sorted.sort((a,b) => a.row - b.row)

        for(let i=0; i<sorted.length;i++)
            theSessionData.moveTimelineItem(sorted[i], distance)
    }

    function focusItems(items) {
        theSessionData.setTimelineFocus(timeline_items.rootIndex, items)
    }

    function rightAlignItems(indexes) {
        theSessionData.alignTimelineItems(indexes, true)
    }

    function leftAlignItems(indexes) {
        theSessionData.alignTimelineItems(indexes, false)
    }

    function moveItemFrames(index, start, duration, dest, insert) {
        theSessionData.moveRangeTimelineItems(index, start, duration, dest, insert)
    }

    function enableItems(indexes, enabled) {
        for(let i=0;i<indexes.length; i++) {
            theSessionData.set(indexes[i], enabled, "enabledRole")
        }
        // updateEnableFlag()
    }

    function lockItems(indexes, locked) {
        for(let i=0;i<indexes.length; i++) {
            theSessionData.set(indexes[i], locked, "lockedRole")
        }
        // updateLockFlag()
    }

    function setItemName(index, name) {
        theSessionData.set(index, name, "nameRole")
    }

    function splitClips(indexes, frame=timelinePlayhead.logicalFrame) {
        if(!indexes.length) {
            let cind = theSessionData.getTimelineClipIndex(timeline_items.rootIndex, timelinePlayhead.logicalFrame)
            if(cind.valid)
                indexes = [cind]
        }

        for(let i=0; i < indexes.length; i++) {
            if(theSessionData.get(indexes[i], "typeRole") == "Clip" && !theSessionData.get(indexes[i], "lockedRole") && !theSessionData.get(indexes[i].parent, "lockedRole"))
                splitClip(indexes[i], theSessionData.get(indexes[i], "trimmedStartRole") + frame - theSessionData.startFrameInParent(indexes[i]))
        }
    }

    function splitClip(index, frame) {
        return theSessionData.splitTimelineClip(frame, index)
    }

    // function handleDrop(before, drop) {
    //     if(drop.hasUrls) {
    //         for(var i=0; i < drop.urls.length; i++) {
    //             if(drop.urls[i].toLowerCase().endsWith('.xst') || drop.urls[i].toLowerCase().endsWith('.xsz')) {
    //                 // Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
    //                 // app_window.sessionFunction.newRecentPath(drop.urls[i])
    //                 return;
    //             }
    //         }
    //     }

    //     // prepare drop data
    //     let data = {}
    //     for(let i=0; i< drop.keys.length; i++){
    //         data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
    //     }

    //     if(before.valid) {
    //         if("xstudio/media-ids" in data) {
    //             let internal_copy = false

    //             // does media exist in our parent.
    //             if(before) {
    //                 let mi = theSessionData.searchRecursive(
    //                     helpers.QVariantFromUuidString(data["xstudio/media-ids"].split("\n")[0]), "idRole"
    //                 )

    //                 if(theSessionData.getPlaylistIndex(before) == theSessionData.getPlaylistIndex(mi)) {
    //                     internal_copy = true
    //                 }
    //             }

    //             if(internal_copy) {
    //                 Future.promise(
    //                     theSessionData.handleDropFuture(Qt.CopyAction, data, before)
    //                 ).then(function(quuids){})
    //             } else {
    //                 media_move_copy_dialog.data = data
    //                 media_move_copy_dialog.index = before
    //                 media_move_copy_dialog.open()
    //             }

    //         } else if("xstudio/timeline-ids" in data) {
    //             let internal_copy = false

    //             // does media exist in our parent.
    //             if(before) {
    //                 let mi = theSessionData.searchRecursive(
    //                     helpers.QVariantFromUuidString(data["xstudio/timeline-ids"].split("\n")[0]), "idRole"
    //                 )

    //                 if(theSessionData.getPlaylistIndex(before) == theSessionData.getPlaylistIndex(mi)) {
    //                     internal_copy = true
    //                 }
    //             }

    //             if(internal_copy) {
    //                 // force move action..
    //                 Future.promise(
    //                     theSessionData.handleDropFuture(Qt.MoveAction, data, before)
    //                 ).then(function(quuids){})
    //             } else {
    //                 console.log("external copy")
    //                 // items from external timeline
    //             }
    //         } else {
    //             Future.promise(
    //                 theSessionData.handleDropFuture(drop.proposedAction, data, before)
    //             ).then(function(quuids){})
    //         }
    //     }
    // }

    XsTimer {
        id: updateRegionTimer
        property var region: null

        interval: 100
        running: false
        repeat: false
        onTriggered: updateRegionSelection(region.x, region.y, region.width, region.height, region.mode)
    }


    function updateRegionSelection(x, y, width, height, mode) {
        // video clips.

        let pv = mapToItem(list_view.itemAtIndex(0).list_view_video, x, y)

        let vleft = pv.x - trackHeaderWidth + list_view.itemAtIndex(0).list_view_video.cX
        let vtop = pv.y - list_view.itemAtIndex(0).list_view_video.footerHeight + list_view.itemAtIndex(0).list_view_video.cY
        let vright = vleft + width - 1

        // we need to be careful we're not selecting off screen items
        let vmax =  list_view.itemAtIndex(0).list_view_video.height + list_view.itemAtIndex(0).list_view_video.cY - list_view.itemAtIndex(0).list_view_video.footerHeight
        let vbottom = Math.min(vtop + height - 1, vmax)

        let vclips = theSessionData.getTimelineVideoClipIndexesFromRect(
            timeline_items.rootIndex,
            vleft,
            vtop,
            vright,
            vbottom,
            scaleX,
            (scaleY*itemHeight)+1,
            true
        )

        timelineSelection.select(
            helpers.createItemSelection(vclips),
            mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : mode & Qt.ControlModifier ? ItemSelectionModel.Select : ItemSelectionModel.ClearAndSelect)

        let vmedia = []
        for(let i=0;i<vclips.length;i++) {
            let ind = getMediaIndex(vclips[i])
            if(ind.valid && !vmedia.includes(ind))
                vmedia.push(ind)
        }

        if(timelinePlayheadSelectionIndex.valid)
            theSessionData.updateSelection(timelinePlayheadSelectionIndex, vmedia, mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : mode & Qt.ControlModifier ? ItemSelectionModel.Select : ItemSelectionModel.ClearAndSelect)


        // mediaSelectionModel.select(
        //     helpers.createItemSelection(vmedia),
        //     mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : mode & Qt.ControlModifier ? ItemSelectionModel.Select : ItemSelectionModel.ClearAndSelect)

        // audio clips.
        let pa = mapToItem(list_view.itemAtIndex(0).list_view_audio, x, y)

        let aleft = pa.x - trackHeaderWidth + list_view.itemAtIndex(0).list_view_audio.cX
        let atop = pa.y + list_view.itemAtIndex(0).list_view_audio.cY + list_view.itemAtIndex(0).list_view_video.count * (scaleY*itemHeight)+1
        let aright = aleft + width - 1
        let abottom = atop + height - 1

        let amin = list_view.itemAtIndex(0).list_view_audio.cY + list_view.itemAtIndex(0).list_view_video.count * (scaleY*itemHeight)+1
        atop = Math.max(atop, amin)

        let aclips = theSessionData.getTimelineAudioClipIndexesFromRect(
            timeline_items.rootIndex,
            aleft,
            atop,
            aright,
            abottom,
            scaleX,
            (scaleY*itemHeight)+1,
            true
        )

        timelineSelection.select(
            helpers.createItemSelection(aclips),
            mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : ItemSelectionModel.Select)

        let amedia = []
        for(let i=0;i<aclips.length;i++) {
            let ind = getMediaIndex(aclips[i])
            if(ind.valid && !amedia.includes(ind))
                amedia.push(ind)
        }

        if(timelinePlayheadSelectionIndex.valid)
            theSessionData.updateSelection(timelinePlayheadSelectionIndex, amedia,
                mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : ItemSelectionModel.Select)

        // mediaSelectionModel.select(
        //     helpers.createItemSelection(amedia),
        //     mode & Qt.ShiftModifier ? ItemSelectionModel.Deselect : ItemSelectionModel.Select)
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

                        // clamp to video window..

                        let listview_pos = item.mapToItem(item.list_view_video, local_x, local_y)

                        if(listview_pos.y < item.list_view_video.height) {
                            child_item = item.list_view_video.itemAt(listview_pos.x + item.list_view_video.contentX , listview_pos.y + item.list_view_video.contentY)
                        }
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
        return item_index.model.index(item_index.row + 1, 0, item_index.parent)
    }

    function preceedingIndex(item_index) {
        return item_index.model.index(item_index.row - 1, 0, item_index.parent)
    }

    XsHotkeyArea {
        id: hotkey_area
        anchors.fill: parent
        context: "timeline"
        focus: true
    }

    Keys.forwardTo: hotkey_area

    XsHotkey {
        context: "timeline"
        sequence:  "Ctrl+Z"
        name: "Timeline Redo"
        description: "Re-does the last undone edit in the timeline"
        onActivated: {
            redo(viewedMediaSetProperties.index);
        }
        componentName: "Timeline"
    }

    XsHotkey {
        context: "timeline"
        sequence:  "SHIFT+C"
        name: "Change Clip Colour"
        description: "Change Active Clip Colour"
        onActivated: {
            let clipIndex = theSessionData.getTimelineClipIndex(timeline_items.rootIndex, timelinePlayhead.logicalFrame);
            if(clipIndex.valid) {
                let colours = [
                    "#FFFF0000",
                    "#FF00FF00",
                    "#FF0000FF",
                    "#FFFFFF00",
                    "#FFFFA500",
                    "#FF800080",
                    "#FF000000",
                    "#FFFFFFFF",
                    "",
                ]
                let current = theSessionData.get(clipIndex, "flagColourRole")
                let colour = colours[0]
                if(current) {
                    for(let i = 0;i<colours.length; i++) {
                        if(colours[i] == current)  {
                            colour = colours[i+1]
                            break
                        }
                    }
                }

                theSessionData.set(clipIndex, colour, "flagColourRole")
            }
        }
        componentName: "Timeline"
    }


    XsHotkey {
        context: "timeline"
        sequence:  "Ctrl+U"
        name: "Timeline Undo"
        description: "Jumps to the end frame"
        onActivated: {
            undo(viewedMediaSetProperties.index);
        }
        componentName: "Timeline"
    }

    XsHotkey {
        context: "timeline"
        sequence:  "Z"
        name: "Timeline Zoom"
        description: "Enables timeline zooming mode"
        onActivated: scalingModeActive = true
        onReleased: scalingModeActive = false
        componentName: "Timeline"
    }

    function updateItemSelectionHorizontal(l,r) {
        timeline.timelineSelection.select(helpers.createItemSelection(
                theSessionData.modifyItemSelectionHorizontal(timeline.timelineSelection.selectedIndexes, l, r)
            ), ItemSelectionModel.ClearAndSelect)
    }

    function updateItemSelectionVertical(u,d) {
        timeline.timelineSelection.select(helpers.createItemSelection(
                theSessionData.modifyItemSelectionVertical(timeline.timelineSelection.selectedIndexes, u, d)
            ), ItemSelectionModel.ClearAndSelect)
    }

    XsHotkey {
        id: select_up_hotkey
        context: "timeline"
        sequence:  "PgUp"
        name: "Move Selection Up"
        description: "Move Selection Up"
        onActivated: updateItemSelectionVertical(1,-1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: select_down_hotkey
        context: "timeline"
        sequence:  "PgDown"
        name: "Move Selection Down"
        description: "Move Selection Down"
        onActivated: updateItemSelectionVertical(-1,1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_up_hotkey
        context: "timeline"
        sequence:  "Ctrl+PgUp"
        name: "Expand Selection Up"
        description: "Expand Selection Up"
        onActivated: updateItemSelectionVertical(1,0)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_down_hotkey
        context: "timeline"
        sequence:  "Ctrl+PgDown"
        name: "Expand Selection Down"
        description: "Expand Selection Down"
        onActivated: updateItemSelectionVertical(0,1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_up_hotkey
        context: "timeline"
        sequence:  "Shift+PgUp"
        name: "Contract Selection Up"
        description: "Contract Selection Up"
        onActivated: updateItemSelectionVertical(-1,0)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_down_hotkey
        context: "timeline"
        sequence:  "Shift+PgDown"
        name: "Contract Selection Down"
        description: "Contract Selection Down"
        onActivated: updateItemSelectionVertical(0,-1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_up_down_hotkey
        context: "timeline"
        sequence:  "Alt+PgUp"
        name: "Expand Selection Up and Down"
        description: "Expand Selection Up and Down"
        onActivated: updateItemSelectionVertical(1,1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_up_down_hotkey
        context: "timeline"
        sequence:  "Alt+PgDown"
        name: "Contract Selection Up and Down"
        description: "Contract Selection Up and Down"
        onActivated: updateItemSelectionVertical(-1,-1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: select_next_hotkey
        context: "timeline"
        sequence:  "DOWN"
        name: "Move Selection Right"
        description: "Move Clip Selection Right"
        onActivated: updateItemSelectionHorizontal(-1,1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: select_previous_hotkey
        context: "timeline"
        sequence:  "UP"
        name: "Move Selection Left"
        description: "Move Clip Selection Left"
        onActivated: updateItemSelectionHorizontal(1,-1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_next_hotkey
        context: "timeline"
        sequence:  "Ctrl+DOWN"
        name: "Expand Selection Right"
        description: "Expand Clip Selection Right"
        onActivated: updateItemSelectionHorizontal(0,+1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_previous_hotkey
        context: "timeline"
        sequence:  "Ctrl+UP"
        name: "Expand Selection Left"
        description: "Expand Clip Selection Left"
        onActivated: updateItemSelectionHorizontal(+1, 0)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_next_hotkey
        context: "timeline"
        sequence:  "Shift+DOWN"
        name: "Contract Selection Right"
        description: "Contract Clip Selection Right"
        onActivated: updateItemSelectionHorizontal(0, -1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_previous_hotkey
        context: "timeline"
        sequence:  "Shift+UP"
        name: "Contract Selection Left"
        description: "Contract Clip Selection Left"
        onActivated: updateItemSelectionHorizontal(-1, 0)
        componentName: "Timeline"
    }

    XsHotkey {
        id: expand_both_hotkey
        context: "timeline"
        sequence:  "Alt+DOWN"
        name: "Expand Selection"
        description: "Expand Selection"
        onActivated: updateItemSelectionHorizontal(1, 1)
        componentName: "Timeline"
    }

    XsHotkey {
        id: contract_both_hotkey
        context: "timeline"
        sequence:  "Alt+UP"
        name: "Contract Selection"
        description: "Contract Clip Selection"
        onActivated: updateItemSelectionHorizontal(-1, -1)
        componentName: "Timeline"
    }

    XsHotkey {
        context: "timeline"
        sequence:  "X"
        name: "Timeline Scroll"
        description: "Enables timeline scrolling mode"

        onActivated: scrollingModeActive = true
        onReleased: scrollingModeActive = false

        componentName: "Timeline"
    }

    XsHotkey {
        context: "timeline"
        sequence:  "F"
        name: "Timeline fit"
        description: "Fits the timeline view to selected items"
        onActivated: fitItems(timeline.timelineSelection.selectedIndexes)
        componentName: "Timeline"
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
            // preventStealing: true

            property bool isScaling: false
            property bool isScrolling: false
            property bool isRegionSelection: false

            property var initialPosition: Qt.point(0,0)
            property var initialValue: 0
            property real minScaleX: 0

            Rectangle {
                id: region
                visible: ma.isRegionSelection
                color: helpers.alphate(XsStyleSheet.accentColor,0.2)
                border.color: XsStyleSheet.accentColor
                border.width: 1
                width: 0
                height: 0
                z: 10

                property int clickX: 0
                property int clickY: 0
                property var mode: null

                function setClick(mouseX, mouseY, modifiers) {
                    clickX = mouseX
                    clickY = mouseY
                    mode = modifiers
                    update(mouseX, mouseY)
                }

                function update(mouseX, mouseY) {
                    if(mouseX < clickX) {
                        x = mouseX
                        width = clickX - x
                    } else {
                        x = clickX
                        width = mouseX - x
                    }

                    if(mouseY < clickY) {
                        y = mouseY
                        height = clickY - y
                    } else {
                        y = clickY
                        height = mouseY - y
                    }
                }
            }


            onContainsMouseChanged: {
                if(containsMouse) {
                    forceActiveFocus()
                }
            }

            function mousePressed(button, mouseX, mouseY, modifiers, pressed=false) {
                if(button == Qt.RightButton) {
                    if(editMode != "Select")
                        adjustSelection(button, mouseX, mouseY, modifiers)

                    if(!hovered || !hovered.isSelected) {
                        adjustSelection(button, mouseX, mouseY, modifiers)
                    }

                    if(!timelineSelection.selectedIndexes.length) {
                        showTimelineMenu(mouseX, mouseY)
                    } else {
                        // check current hovered is part of selection..
                        let ctype = timelineSelection.selectedIndexes[0].model.get(timelineSelection.selectedIndexes[0], "typeRole")
                        if(ctype == "Clip")
                            showClipMenu(mouseX, mouseY)//-timelineMenu.height)
                        else if (["Audio Track", "Video Track"].includes(ctype))
                            showTrackMenu(mouseX, mouseY)//-timelineMenu.height)
                    }
                } else if(button == Qt.LeftButton) {
                    if(pressed && scalingModeActive) {
                        isScaling = scalingModeActive
                        minScaleX = ((list_view.width - trackHeaderWidth) / theSessionData.timelineRect([timeline_items.rootIndex]).width)/2
                        initialValue = scaleX
                        initialPosition = Qt.point(mouseX, mouseY)
                    } else if(pressed && scrollingModeActive) {
                        isScrolling = scrollingModeActive
                        initialValue = list_view.itemAtIndex(0).currentPosition()
                        initialPosition = Qt.point(mouseX, mouseY)
                    } else if(pressed && editMode == "Cut" && modifiers == Qt.NoModifier && hovered.itemTypeRole == "Clip") {
                        if(!theTimeline.timelineSelection.selectedIndexes.includes(hovered.modelIndex()))
                            adjustSelection(button, mouseX, mouseY, modifiers)
                        splitClips(theTimeline.timelineSelection.selectedIndexes, cutLine)
                        snapCacheKey = helpers.makeQUuid()
                    } else if(pressed && mouseX > trackHeaderWidth) {
                        region.setClick(mouseX, mouseY, modifiers)
                        isRegionSelection = true
                    } else {
                        adjustSelection(button, mouseX, mouseY, modifiers)
                    }
                }

            }

            onPressed: mousePressed(mouse.button, mouse.x, mouse.y, mouse.modifiers, true)

            function tapped(button, mouseX, mouseY, modifiers, item=null) {
                let l = mapFromGlobal(mouseX, mouseY)
                mousePressed(button, l.x, l.y, modifiers)
            }

            function doubleTapped(item=null, mode=null) {
                // jump to playlist and show media.
                if(hovered && hovered.mediaUuid) {

                    // set the aux playhead to drive the viewer - it will not
                    // show the timeline but just the bit of media we have
                    // double clicked on
                    viewportCurrentMediaContainerIndex = currentMediaContainerIndex

                    callbackTimer.setTimeout(function() { return function() {
                        // we 'un-pin' the timeline playhead so that instead of using
                        // the timline as its source it uses the selected media alone
                        currentPlayhead.pinnedSourceMode = false
                    }}(), 200);

                }
            }

            onDoubleClicked: doubleTapped()

            onReleased: {
                isScaling = false
                isScrolling = false

                if(isRegionSelection) {
                    isRegionSelection = false
                    if(region.width < 5 && region.height < 5) {
                        adjustSelection(mouse.button, mouse.x, mouse.y, mouse.modifiers)
                    }
                }

                moveDragHandler.enabled = false
            }

            onPositionChanged: {
                if(isScaling) {
                    // cap min scale to fit timeline.
                    // scaleX = Math.max(minScaleX, initialValue - ((initialPosition.x - mouse.x)/40.0))
                    // if(initialPosition.x - mouse.x > 0)
                    //     scaleX = Math.max(minScaleX, initialValue + ((initialValue/100) * (initialPosition.x - mouse.x)))
                    // else
                    let steps = Math.abs(initialPosition.x - mouse.x)
                    let initial = initialValue
                    for(let i=0; i< steps; i++) {
                        if(initialPosition.x - mouse.x > 0)
                            initial -= (initial*0.004)
                        else
                            initial += (initial*0.004)
                    }
                    scaleX = Math.max(minScaleX, initial)
                        // scaleX = Math.max(minScaleX, initialValue - ((initialValue/100) * (initialPosition.x - mouse.x)))
                    // reset value so increasing works imediately.
                    if(scaleX == minScaleX) {
                        initialPosition = Qt.point(mouse.x, mouse.y)
                        initialValue = scaleX
                    }
                    list_view.itemAtIndex(0).jumpToFrame(timelinePlayhead.logicalFrame, ListView.Contain)
                } else if (isScrolling) {
                    let np = (list_view.itemAtIndex(0).scrollbar.size / list_view.itemAtIndex(0).scrollbar.width) * (initialPosition.x - mouse.x)
                    let rnp = list_view.itemAtIndex(0).jumpToPosition(initialValue + np)

                    // reset if we hit bounds.
                    if(rnp != (initialValue + np)) {
                        initialValue = rnp
                        initialPosition = Qt.point(mouse.x, mouse.y)
                    }
                } else {
                    if(isRegionSelection) {
                        region.update(mouse.x, mouse.y)
                        updateRegionTimer.region = region
                        if(!updateRegionTimer.running)
                            updateRegionTimer.start()
                    } else {
                        if(editMode == "Move" || editMode == "Roll") {
                            showHandles(mouse.x, mouse.y)
                        } else if(editMode == "Cut") {
                            // calculate timeline frame posisiton.
                            // from mouse position.
                            let stackItem = list_view.itemAtIndex(0)
                            let newCut = stackItem.cursor.start + (((mouse.x - trackHeaderWidth) + stackItem.cursor.fractionOffset) / stackItem.cursor.tickWidth)

                            if(snapMode) {
                                let matched = theSessionData.snapTo(
                                    timeline_items.rootIndex,
                                    list_view.playheadFrame,
                                    newCut, 0, 0, 10 / scaleX, snapCacheKey)
                                if(matched.length) {
                                    newCut += matched[0]
                                }
                            }

                            cutLine = newCut
                            let [item, item_type, local_x, local_y] = resolveItem(mouse.x, mouse.y)

                            if(hovered != item)
                                hovered = item

                        } else {
                            let [item, item_type, local_x, local_y] = resolveItem(mouse.x, mouse.y)

                            if(hovered != item)
                                hovered = item
                        }
                    }
                }
            }

            onWheel: {
                // maintain position as we zoom..
                if(wheel.modifiers == Qt.ShiftModifier) {
                    if(wheel.angleDelta.y > 1) {
                        scaleY += 0.2
                    } else {
                        scaleY -= 0.2
                    }
                    wheel.accepted = true
                } else if(wheel.modifiers == Qt.ControlModifier) {
                    let tmp = scaleX
                    if(wheel.angleDelta.y > 1) {
                        tmp += 0.2
                    } else {
                        tmp -= 0.2
                    }
                    scaleX = Math.max((list_view.width - trackHeaderWidth) / theSessionData.timelineRect([timeline_items.rootIndex]).width, tmp)
                    list_view.itemAtIndex(0).jumpToFrame(timelinePlayhead.logicalFrame, ListView.Center)
                    wheel.accepted = true
                } else if(hovered != null && ["Video Track", "Audio Track","Gap","Clip"].includes(hovered.itemTypeRole)) {
                    if(["Video Track", "Audio Track"].includes(hovered.itemTypeRole))
                        hovered.parentLV.flick(0, wheel.angleDelta.y > 1 ? 500 : -500)
                    else if(["Gap", "Clip"].includes(hovered.itemTypeRole))
                        hovered.parentLV.parentLV.flick(0, wheel.angleDelta.y > 1 ? 500 : -500)
                    wheel.accepted = true
                } else {
                    wheel.accepted = false
                }
            }

            function showHandles(mousex, mousey) {
                let [item, item_type, local_x, local_y] = resolveItem(mousex, mousey)

                if(hovered != item) {
                    if(hovered && hovered.itemTypeRole == "Clip")
                        theSessionData.resetTimelineItemDragFlag([hovered.modelIndex()]);
                    hovered = item

                    if(hovered) {
                        if(["Select"].includes(editMode))
                            return

                        if("Clip" == item_type)
                            theSessionData.updateTimelineItemDragFlag([hovered.modelIndex()], editMode == "Roll", rippleMode, overwriteMode)
                    }
                }
            }

            function draggingStarted(index, item, mode) {
                if(!timelineSelection.selectedIndexes.includes(index)) {
                    timelineSelection.select(index, ItemSelectionModel.ClearAndSelect)
                }

                snapCacheKey = helpers.makeQUuid()
                if(mode == "left")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode, rippleMode, overwriteMode)
                else if(mode == "right")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode, rippleMode, overwriteMode)
                else if(mode == "leftleft")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode)
                else if(mode == "rightright")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode)
                else if(mode == "middle")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode, rippleMode, overwriteMode)
                else if(mode == "roll")
                    theSessionData.beginTimelineItemDrag(timelineSelection.selectedIndexes, mode)
            }

            function dragging(index, item, mode, x, y) {
                if(snapMode) {
                    let clipStart = theSessionData.startFrameInParent(index)
                    let matched = []
                    if(mode == "left" || mode == "leftleft") {
                        matched = theSessionData.snapTo(
                            index, list_view.playheadFrame,
                            clipStart,
                            0,
                            x,
                            10 / scaleX,
                            snapCacheKey
                        )
                    } else if(mode == "middle") {
                        matched = theSessionData.snapTo(
                            index, list_view.playheadFrame,
                            clipStart,
                            item.currentDurationFrame,
                            x,
                            10 / scaleX,
                            snapCacheKey
                        )
                    } else if(mode == "right" || mode == "rightright") {
                        matched = theSessionData.snapTo(
                            index, list_view.playheadFrame,
                            clipStart+item.currentDurationFrame,
                            0,
                            x,
                            10 / scaleX,
                            snapCacheKey
                        )
                    }

                    if(matched.length) {
                        x = matched[0]
                        if(mode == "right" || mode == "rightright") {
                            if(matched[2])
                                snapLine = clipStart+item.currentDurationFrame + x
                            else
                                snapLine = clipStart+item.currentDurationFrame + item.currentDurationFrame + x
                        } else {
                            if(matched[2])
                                snapLine = clipStart + x
                            else
                                snapLine = clipStart + item.currentDurationFrame + x
                        }
                    } else {
                        snapLine = -1
                    }
                }

                if(mode == "left")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, 0, rippleMode, overwriteMode)
                else if(mode == "right")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, 0, rippleMode, overwriteMode)
                else if(mode == "leftleft")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, 0)
                else if(mode == "rightright")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, 0)
                else if(mode == "middle")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, y, rippleMode, overwriteMode)
                else if(mode == "roll")
                    theSessionData.updateTimelineItemDrag(timelineSelection.selectedIndexes, mode, x, 0)
            }


            function _Timer() {
                 return Qt.createQmlObject("import QtQuick 2.0; Timer {}", appWindow);
            }

            function delayCallback(delayTime, cb) {
                 let timer = new _Timer();
                 timer.interval = delayTime;
                 timer.repeat = false;
                 timer.triggered.connect(cb);
                 timer.start();
            }

            function draggingStoppedReal(index, item, mode) {
                if(mode == "left")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode, overwriteMode)
                else if(mode == "right")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode, overwriteMode)
                else if(mode == "leftleft")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode)
                else if(mode == "rightright")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode)
                else if(mode == "middle")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode, overwriteMode)
                else if(mode == "roll")
                    theSessionData.endTimelineItemDrag(timelineSelection.selectedIndexes, mode)
                snapLine = -1
            }

            // this function cannot remove the calling item before the handler returns.
            // so the gut's of this function need to be called after.

            // Object 0x4ff6500 destroyed while one of its QML signal handlers is in progress.
            // Most likely the object was deleted synchronously (use QObject::deleteLater() instead), or the application is running a nested event loop.
            // This behavior is NOT supported!

            function draggingStopped(index, item, mode) {
                delayCallback(10, function() {
                   draggingStoppedReal(index, item, mode)
                })
            }

            function isValidSelection(ctype, ntype) {
                // let result = false
                // if(["Clip","Gap"].includes(ctype) && ["Clip","Gap"].includes(ntype))
                //     result = true
                // else
                //     result = ctype == ntype

                return ctype == ntype
            }

            function adjustSelection(button, x, y, modifiers) {
                if(hovered != null) {
                    if(button == Qt.RightButton && hovered.isSelected) {
                        // ignored
                    } else if(button == Qt.RightButton && modifiers != Qt.ControlModifier) {
                        selectItem()
                    } else {
                        if (modifiers == Qt.ShiftModifier) {
                            // validate selection, we don't allow mixed items..
                            let isValid = true
                            if(timelineSelection.selectedIndexes.length) {
                                let ctype = timelineSelection.selectedIndexes[0].model.get(timelineSelection.selectedIndexes[0], "typeRole")
                                isValid = isValidSelection(ctype, hovered.itemTypeRole)
                            }
                            if(isValid) {
                                let sel = timelineSelection.selectedIndexes
                                if(sel.length) {
                                    let index = hovered.modelIndex()
                                    // find last selected entry ?
                                    let m = sel[sel.length-1]
                                    if(m != index) {
                                        let s = Math.min(index.row, m.row)
                                        let e = Math.max(index.row, m.row)
                                        let items = []
                                        let mitems = []

                                        // ignore gaps.. ?
                                        for(let i=s; i<=e; i++) {
                                            let nindex = timelineSelection.model.index(i, 0, index.parent)
                                            let itype = timelineSelection.model.get(nindex, "typeRole")

                                            if(["Clip","Audio Track", "Video Track"].includes(itype)) {
                                                items.push(nindex)

                                                if(itype == "Clip") {
                                                    let mindex = getMediaIndex(nindex)
                                                    if(mindex.valid && !mitems.includes(mindex))
                                                        mitems.push(mindex)
                                                }
                                            }
                                        }
                                        timelineSelection.select(helpers.createItemSelection(items), ItemSelectionModel.ClearAndSelect)

                                        if(timelinePlayheadSelectionIndex.valid)
                                            theSessionData.updateSelection(timelinePlayheadSelectionIndex, mitems, ItemSelectionModel.ClearAndSelect)

                                        // mediaSelectionModel.select(helpers.createItemSelection(mitems), ItemSelectionModel.ClearAndSelect)
                                    }
                                } else {
                                    selectItem()
                                }
                            }
                        } else if (modifiers == Qt.ControlModifier) {
                            // validate selection, we don't allow mixed items..
                            let isValid = true
                            if(timelineSelection.selectedIndexes.length) {
                                let ctype = timelineSelection.selectedIndexes[0].model.get(timelineSelection.selectedIndexes[0], "typeRole")
                                isValid = isValidSelection(ctype, hovered.itemTypeRole)
                            }
                            if(isValid && button != Qt.RightButton) {
                                let new_state = hovered.isSelected  ? ItemSelectionModel.Deselect : ItemSelectionModel.Select
                                if(["Clip","Audio Track", "Video Track"].includes(hovered.itemTypeRole)) {
                                    timelineSelection.select(hovered.modelIndex(), new_state)

                                    if(hovered.itemTypeRole == "Clip" && hovered.mediaIndex.valid && timelinePlayheadSelectionIndex.valid) {
                                        theSessionData.updateSelection(
                                            timelinePlayheadSelectionIndex,
                                            [hovered.mediaIndex],
                                            new_state
                                        )
                                        // mediaSelectionModel.select(hovered.mediaIndex, new_state)
                                    }
                                }
                            }
                        } else if(modifiers == Qt.NoModifier) {
                            selectItem()
                        }
                    }
                }
            }

            function selectItem(dragging=false) {
                if(hovered.itemTypeRole == "Clip" && hovered.hasMedia) {
                    // find media in media list and select ?
                    let mind = hovered.mediaIndex
                    if(mind.valid && timelinePlayheadSelectionIndex.valid) {
                        theSessionData.updateSelection(timelinePlayheadSelectionIndex, [mind])
                    }
                    // mediaSelectionModel.select(mind, ItemSelectionModel.ClearAndSelect)
                }

                if(dragging) {
                    if("Clip" == hovered.itemTypeRole)
                        timelineSelection.select(hovered.modelIndex(), ItemSelectionModel.Select)
                } else if(["Clip","Audio Track", "Video Track", "Gap"].includes(hovered.itemTypeRole))
                    timelineSelection.select(hovered.modelIndex(), ItemSelectionModel.ClearAndSelect)
                else
                    timelineSelection.clear()
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
                model: DelegateModel {
                    // id: timeline_items
                    property var srcModel: theSessionData
                    model: srcModel
                    rootIndex: helpers.qModelIndex()
                    delegate: DelegateChooser {
                        role: "typeRole"

                        DelegateChoice {
                            roleValue: "Stack"
                            XsDelegateStack {}
                        }
                        DelegateChoice {
                            Item {}
                        }
                    }
                }
                orientation: ListView.Horizontal

                property var timelineItem: timeline
                property var hoveredItem: hovered
                property real scaleX: timeline.scaleX
                property real scaleY: timeline.scaleY
                property real itemHeight: timeline.itemHeight
                property real trackHeaderWidth: timeline.trackHeaderWidth
                property var setTrackHeaderWidth: timeline.setTrackHeaderWidth
                property var timelineSelection: timeline.timelineSelection
                property int playheadFrame: timelinePlayhead.logicalFrame ? timelinePlayhead.logicalFrame : 0
                property string itemFlag: defaultClip

                property var draggingStarted: ma.draggingStarted
                property var dragging: ma.dragging
                property var draggingStopped: ma.draggingStopped
                property var doubleTapped: ma.doubleTapped
                property var tapped: ma.tapped

                onPlayheadFrameChanged: {
                    if (itemAtIndex(0))
                        itemAtIndex(0).jumpToFrame(playheadFrame, ListView.Visible)
                }
            }
        }
    }

    XsClipDragBoth {
        id: dragInsert
        visible: false
        x: 0
        y: 0
        width: 20
        height: itemHeight * scaleY
        thickness: 2
        z:10
    }

    XsClipDragLeft {
        id: dragPrepend
        visible: false
        x: 0
        y: 0
        width: 10
        height: itemHeight * scaleY
        thickness: 2
        z:10
    }

    XsClipDragReplace {
        id: dragReplace
        visible: false
        x: 0
        y: 0
        width: 10
        height: itemHeight * scaleY
        thickness: 2
        z:10
    }

    XsClipDragRight {
        id: dragAppend
        visible: false
        x: 0
        y: 0
        width: 20
        height: itemHeight * scaleY
        thickness: 2
        z:10
    }

    /*XsMediaMoveCopyDialog {
        id: media_move_copy_dialog
    }*/

    XsDragDropHandler {

        id: drag_drop_handler
        property bool dragTarget: false
        property var modelIndex: null
        property bool newVideoTrack: true

        onDragEntered: {
            // console.log(source, data)
            if (source == "MediaList" && typeof data == "object" && data.length) {
                dragTarget = true
                processPosition(mousePosition.x, mousePosition.y, data)
            }
        }

        onDragExited: {
            if(dragTarget) {
                dragTarget = false
                modelIndex = null
                dragInsert.visible = false
                dragPrepend.visible = false
                dragAppend.visible = false
                dragReplace.visible = false
            }
        }

        onDragged: {
            if(dragTarget)
                processPosition(mousePosition.x, mousePosition.y, data)
        }

		onDropped: {
			if (dragTarget) {
                processPosition(mousePosition.x, mousePosition.y, data)

                if(modelIndex) {
                    // determine what we need to do..
                    let type = modelIndex.model.get(modelIndex, "typeRole")
                    let new_indexes = theSessionData.moveRows(
                        data,
                        -1, // insertion row: make invalid so always inserts on the end
                        timeline_items.rootIndex.parent,
                        true
                    )
                    if(dragReplace.visible) {
                        // replace clip media.
                        modelIndex.model.set(
                            modelIndex,
                            modelIndex.model.get(new_indexes[0],"actorUuidRole"),
                            "clipMediaUuidRole"
                        )
                    } else {
                        let clipRow = 0;
                        let clipParent = null

                        if(type == "Video Track" || type == "Audio Track") {
                            // append
                            clipParent = modelIndex
                            clipRow = modelIndex.model.rowCount(modelIndex)
                        } else if(type == "Stack") {
                            // new track, but which ? audio or video..
                            if(newVideoTrack) {
                                clipParent = addTrack("Video Track")[0]
                                clipRow = 0
                            } else {
                                clipParent = addTrack("Audio Track")[0]
                                clipRow = 0
                            }
                        } else {
                            clipParent = modelIndex.parent
                            clipRow = modelIndex.row
                        }

        				for (var c = 0; c < new_indexes.length; ++c) {
                            // must add to container..
        					theSessionData.insertTimelineClip(c + clipRow, clipParent, new_indexes[c], "")
        				}
                    }
                    modelIndex = null
                }

                dragInsert.visible = false
                dragPrepend.visible = false
                dragAppend.visible = false
                dragReplace.visible = false
                dragTarget = false
			}
        }

        function processPosition(x, y, data) {
            let [item, item_type, local_x, local_y] = resolveItem(x, y)
            let handle = 16
            let show_dragPrepend = false
            let show_dragAppend = false
            let show_dragInsert = false
            let show_dragReplace = false

            // update ovelay to indicate drop location.
            if(item) {
                if(["Clip", "Gap"].includes(item_type)) {
                    if(local_x >= 0 && local_x < handle) {
                        modelIndex = item.modelIndex()
                        let ppos = mapFromItem(item, 0, 0)
                        let item_row = modelIndex.row

                        if(item_row) {
                            dragInsert.x = ppos.x -dragInsert.width / 2
                            dragInsert.y = ppos.y
                            show_dragInsert = true
                        } else {
                            dragPrepend.x = ppos.x
                            dragPrepend.y = ppos.y
                            show_dragPrepend = true
                        }
                    }
                    else if(local_x >= item.width - handle && local_x < item.width) {
                        let ppos = mapFromItem(item, item.width, 0)
                        let item_row = item.modelIndex().row

                        if(item_row == item.modelIndex().model.rowCount(item.modelIndex().parent)-1) {
                            dragAppend.x = ppos.x - dragAppend.width
                            dragAppend.y = ppos.y
                            show_dragAppend = true
                            modelIndex = item.modelIndex().parent
                        } else {
                            dragInsert.x = ppos.x - (dragInsert.width / 2)
                            dragInsert.y = ppos.y
                            show_dragInsert = true
                            modelIndex = item.modelIndex().model.index(item_row+1,0,item.modelIndex().parent)
                        }
                    } else if(data.length == 1 && item_type == "Clip"){
                        let ppos = mapFromItem(item, 0, 0)
                        dragReplace.x = ppos.x
                        dragReplace.y = ppos.y
                        dragReplace.width = item.width
                        show_dragReplace = true
                        modelIndex = item.modelIndex()
                    }
                } else if(["Audio Track", "Video Track"].includes(item_type)) {
                    // prepend / append...
                    // always append unless empty.
                    modelIndex = item.modelIndex()

                    if(modelIndex.model.rowCount(modelIndex)) {
                        // calculate track width
                        let frames = modelIndex.model.get(modelIndex, "trimmedDurationRole")
                        let ppos = mapFromItem(item, frames*scaleX, 0)
                        dragAppend.x = ppos.x - dragAppend.width + trackHeaderWidth
                        dragAppend.y = ppos.y
                        show_dragAppend = true
                    } else {
                        let ppos = mapFromItem(item, 0, 0)

                        dragPrepend.x = trackHeaderWidth
                        dragPrepend.y = ppos.y
                        show_dragPrepend = true
                    }
                } else if(item_type == "Stack") {
                    // new track..
                    //  find first vid / last audio
                    //  which are we nearest to ?
                    let prepend_y = 0
                    if(y < mapFromItem(item.list_view_middle, 0, 0).y) {
                        // video
                        prepend_y = mapFromItem(item.list_view_video.footerItem, 0, item.list_view_video.footerItem.height).y - dragPrepend.height
                        newVideoTrack = true
                    } else {
                        // audio
                        prepend_y = mapFromItem(item.list_view_audio.footerItem, 0, 0).y
                        newVideoTrack = false
                    }
                    modelIndex = item.modelIndex()
                    dragPrepend.x = trackHeaderWidth
                    dragPrepend.y = prepend_y
                    show_dragPrepend = true
                } else {
                    // console.log("ERM")
                }
            }

            if(show_dragPrepend != dragPrepend.visible)
                dragPrepend.visible = show_dragPrepend

            if(show_dragAppend != dragAppend.visible)
                dragAppend.visible = show_dragAppend

            if(show_dragInsert != dragInsert.visible)
                dragInsert.visible = show_dragInsert

            if(show_dragReplace != dragReplace.visible)
                dragReplace.visible = show_dragReplace
        }
	}

    // DropArea {
    //     id: drop_area
    //     keys: [
    //         "text/uri-list",
    //         "xstudio/media-ids",
    //         "xstudio/timeline-ids",
    //         "application/x-dneg-ivy-entities-v1"
    //     ]
    //     anchors.fill: parent

    //     property var modelIndex: null

    //     onEntered: {
    //         console.log("ENTERED")
    //         processPosition(drag.x, drag.y)
    //     }

    //     onExited: {
    //         console.log("EXITED")
    //         modelIndex = null
    //         dragBothLeft.visible = false
    //         dragBothRight.visible = false
    //         dragLeft.visible = false
    //         dragRight.visible = false
    //         moveClip.visible = false
    //     }


    //     onPositionChanged: {
    //         processPosition(drag.x, drag.y)
    //     }

    //     onDropped: {
    //         console.log("DROPPED")
    //         processPosition(drop.x, drop.y)
    //         if(modelIndex != null) {
    //             handleDrop(modelIndex, drop)
    //             modelIndex = null
    //         }
    //         dragBothLeft.visible = false
    //         dragBothRight.visible = false
    //         dragLeft.visible = false
    //         moveClip.visible = false
    //         dragRight.visible = false
    //     }
    // }

}