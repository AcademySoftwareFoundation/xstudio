// SPDX-License-Identifier: Apache-2.0
import QtQuick




import QtQuick.Layouts
import QuickFuture 1.0

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import "."
import "./widgets"
import "./delegates"
import ".."
import "../functions"
import "../widgets"

XsListView {

    id: mediaList

    cacheBuffer: 80
    boundsBehavior: Flickable.StopAtBounds

    property var dragTargetIndex
    property bool dragToEnd: false
    property real itemRowHeight: rowHeight

    property real itemRowWidth: width

    delegate: XsMediaItemDelegate {
        width: itemRowWidth
    }

    model: mediaListModelData

    PropertyAnimation{
        id: autoScrollAnimator
        target: mediaList
        property: "contentY"
        duration: 100
    }

    XsPopupMenu {
        id: flagMenu
        visible: false
        menu_model_name: "media_flag_menu_"+flagMenu
        property var panelContext: helpers.contextPanel(flagMenu)
        property var mediaIndex: null

        XsFlagMenuInserter {
            text: ""
            menuPath: ""
            panelContext: flagMenu.panelContext
            menuModelName: flagMenu.menu_model_name
            onFlagSet: (flag, flag_text) => {
                theSessionData.set(flagMenu.mediaIndex, flag, "flagColourRole")

                if (flag_text)
                    theSessionData.set(flagMenu.mediaIndex, flag_text, "flagTextRole")
            }
        }
    }

    function showFlagMenu(mx, my, source, mediaIndex) {
        let sp = mapFromItem(source, mx, my)
        flagMenu.x = sp.x
        flagMenu.y = sp.y
        flagMenu.mediaIndex = mediaIndex
        flagMenu.visible = true
    }

    // here we auto-scroll the list so that the on-screen media is visible in
    // the panel
    property var onScreenMediaUuid: currentPlayhead.mediaUuid
    onOnScreenMediaUuidChanged: autoScrollToOnScreenMedia()
    property var playheadPlayingOrScrubbing: currentPlayhead.playing || currentPlayhead.scrubbingFrames
    onPlayheadPlayingOrScrubbingChanged: autoScrollToOnScreenMedia()

    function autoScrollToOnScreenMedia() {

        if (playheadPlayingOrScrubbing || !visible) return

        // what row is the on-screen media?
        var row = mediaListModelData.getRowWithMatchingRoleData(onScreenMediaUuid, "actorUuidRole")
        if (row == -1) return

        // what is the Y coordinate of the media item within the media list?
        var mid = itemRowHeight*row + itemRowHeight/2 + originY

        // already within visible bounds?
        if (mid > contentY && mid < (contentY+mediaList.height) || mediaList.height == 0) return

        autoScrollAnimator.to = Math.max(originY, Math.min(mid - mediaList.height/2, mediaList.contentHeight - mediaList.height))

        autoScrollAnimator.running = true

    }

    XsMediaListMouseArea {
        id: mouseArea
        anchors.fill: parent
        // this margin ensures that this mousearea doesn't steal mouse events
        // from the scrollbar
        anchors.rightMargin: 12
        anchors.leftMargin: 10 // or the flag, guess size ?
    }
    property alias mouseArea: mouseArea

    property alias drag_drop_handler: drag_drop_handler

    XsDragDropHandler {

        id: drag_drop_handler
        dragSourceName: "MediaList"
        dragData: mediaSelectionModel.selectedIndexes

        onIsDragTargetChanged: {
            if (!isDragTarget) {
                scrollUp.cancel()
                scrollDown.cancel()
            }
        }

        onDragged: (mousePosition, source, data) => {
            computeTargetDropIndex(mousePosition.y)
            autoScroll(mousePosition.y)
        }

        onDragEnded: {
            if (dragTargetIndex != undefined && dragTargetIndex.valid) {
                mediaList.itemAtIndex(dragTargetIndex.row).isDragTarget = false
            }
            dragTargetIndex = undefined

            scrollUp.cancel()
            scrollDown.cancel()

        }

        onDropped: (mousePosition, source, data) => {

            scrollUp.cancel()
            scrollDown.cancel()

            var idx = dragTargetIndex
            if (idx == undefined || !idx.valid) {
                idx = mediaListModelDataRoot
                if (idx == undefined || !idx.valid) {
                    idx = theSessionData.createPlaylist(theSessionData.getNextName("Playlist {}"))
                } else {
                    // mediaListModelDataRoot is the 'MediaList' that lives
                    // underneath a Playist, Subset etc. We want to call
                    // handleDropFuture with the Playlist/Subset/Timeline index
                    idx = idx.parent
                }
            }

            if (source == "External URIS") {


                Future.promise(
                    theSessionData.handleDropFuture(
                        Qt.CopyAction,
                        {"text/uri-list": data},
                        dragToEnd ? idx.parent : idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
                dragTargetIndex = undefined
                return

            } else if (source == "External JSON") {

                Future.promise(
                    theSessionData.handleDropFuture(
                        Qt.CopyAction,
                        data,
                        dragToEnd ? idx.parent : idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
                dragTargetIndex = undefined
                return
            }

            if (!dragTargetIndex) return

            if (dragSourceName == "MediaList") {
                // selection being dropped from a media list. 'data' should be
                // a list of model indeces

                // are these indeces from the same list as our list here?
                if (data.length && data[0].parent == mediaListModelDataRoot) {
                    // do a move rows
                    beforeMoveContentY = contentY
                    theSessionData.moveRows(
                        data,
                        dragToEnd ? -1 : dragTargetIndex.row,
                        dragTargetIndex.parent.parent
                        )
                }
            }
        }

    }


    function isInSelection(idx) {
        return mediaSelectionModel.selectedIndexes.includes(mediaListModelData.rowToSourceIndex(idx))
    }

    function computeTargetDropIndex(dropCoordY) {

        var oldDragTarget = dragTargetIndex
        dragToEnd = false

        if (dropCoordY < 0 || dropCoordY > height) {
            dragTargetIndex = undefined
            if (oldDragTarget) {
                mediaList.itemAtIndex(oldDragTarget.row).isDragTarget = false
            }
            return
        }

        var idx = mediaList.indexAt(10, dropCoordY + contentY)
        if (idx == -1 && mediaList.count) {
            dragTargetIndex = mediaList.itemAtIndex(mediaList.count-1).modelIndex()
            dragToEnd = true
        } else if (idx != -1) {
            var y = mediaList.mapToItem(mediaList.itemAtIndex(idx), 10, dropCoordY).y
            if (y > itemRowHeight/2 && idx < mediaList.count) {
                idx = idx+1
            }

            if (idx == mediaList.count) {
                idx--;
                dragToEnd = true
            }

            // the index that we are going to drop items into cannot
            // be one of the selected items. Find the nearest unselected
            // index
            if (isInSelection(idx)) {
                var lidx = idx
                while (isInSelection(lidx)) {
                    lidx = lidx-1
                    if (!lidx) break
                }
                var hidx = idx
                while (isInSelection(hidx)) {
                    if (hidx = (mediaList.count-1)) break
                    hidx = hidx+1
                }

                if ((idx-lidx) < (hidx-idx)) {
                    idx = lidx
                } else {
                    idx = hidx
                }

            }
            if (mediaList.itemAtIndex(idx)) {
                mediaList.itemAtIndex(idx).isDragTarget = true
                dragTargetIndex = mediaList.itemAtIndex(idx).modelIndex()
            }

        } else {
            dragTargetIndex = undefined
        }

        if (oldDragTarget && oldDragTarget != dragTargetIndex) {
            if (mediaList.itemAtIndex(oldDragTarget.row)) {
                mediaList.itemAtIndex(oldDragTarget.row).isDragTarget = false
            }
        }

    }

    // When we do a moveRows, the list view resets contentY to zero. Annoying.
    // So here we try and preserve the scrolled position in that case
    property real beforeMoveContentY: 0
    onContentYChanged: {
        if(beforeMoveContentY != 0){
            contentY = beforeMoveContentY
            beforeMoveContentY = 0
        }
    }

    property var autoScrollVelocity: 200
    function autoScroll(mouseY) {
        if ((height-mouseY) < 0 || mouseY < 0) {
            scrollUp.cancel()
            scrollDown.cancel()
        } else if ((height-mouseY) < 60) {
            scrollUp.cancel()
            scrollDown.run()
        } else if (mouseY < 60) {
            scrollDown.cancel()
            scrollUp.run()
        } else if (scrollUp.running) {
            scrollUp.cancel()
        } else if (scrollDown.running) {
            scrollDown.cancel()
        }
    }

    SmoothedAnimation {
        id: scrollDown
        target: mediaList;
        properties: "contentY";
        velocity: autoScrollVelocity
        to: mediaList.count*itemRowHeight - mediaList.height + originY
        function cancel() {
            if (running) stop()
        }
        function run() {
            if (!running && mediaList.contentY < (mediaList.count*itemRowHeight - mediaList.height + originY)) start()
        }
    }

    SmoothedAnimation {
        id: scrollUp
        target: mediaList;
        properties: "contentY";
        velocity: autoScrollVelocity
        to: originY
        function cancel() {
            if (running) stop()
        }
        function run() {
            if (!running && mediaList.contentY > originY) start()
        }
    }

}
