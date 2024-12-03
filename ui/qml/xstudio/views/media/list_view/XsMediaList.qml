// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15
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
    // model: filteredMediaListData

    property var dragTargetIndex
    property real itemRowHeight: rowHeight

    property real itemRowWidth: width

    delegate: XsMediaItemDelegate {
        width: itemRowWidth
    }

    Rectangle{ id: resultsBg
        anchors.fill: parent
        color: XsStyleSheet.panelBgColor
        z: -1
    }

    model: mediaListModelData

    PropertyAnimation{
        id: autoScrollAnimator
        target: mediaList
        property: "contentY"
        duration: 100
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

    XsLabel {
        anchors.fill: parent
        visible: !mediaList.count
        text: 'Click the "+" button or drop files/folders here to add Media'
        color: XsStyleSheet.hintColor
        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    XsMediaListMouseArea {
        id: mouseArea
        anchors.fill: parent
        // this margin ensures that this mousearea doesn't steal mouse events
        // from the scrollbar
        anchors.rightMargin: 12
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

        onDragged: {
            computeTargetDropIndex(mousePosition.y)
            autoScroll(mousePosition.y)
        }

        onDragEnded: {
            if (dragTargetIndex != undefined && dragTargetIndex.valid) {
                mediaList.itemAtIndex(dragTargetIndex.row).isDragTarget = false
            }
            dragTargetIndex = undefined
        }

        onDropped: {

            var idx = dragTargetIndex
            if (idx == undefined || !idx.valid) {
                idx = mediaListModelDataRoot
                if (idx == undefined || !idx.valid) {
                    idx = theSessionData.createPlaylist("New Playlist")
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
                        idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
                return

            } else if (source == "External JSON") {

                Future.promise(
                    theSessionData.handleDropFuture(
                        Qt.CopyAction,
                        data,
                        idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
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
                        dragTargetIndex.row,
                        dragTargetIndex.parent.parent
                        )
                    dragTargetIndex = undefined
                }
            }
        }

    }


    function isInSelection(idx) {
        return mediaSelectionModel.selectedIndexes.includes(mediaListModelData.rowToSourceIndex(idx))
    }

    function computeTargetDropIndex(dropCoordY) {

        var oldDragTarget = dragTargetIndex

        if (dropCoordY < 0 || dropCoordY > height) {
            dragTargetIndex = undefined
            if (oldDragTarget) {
                mediaList.itemAtIndex(oldDragTarget.row).isDragTarget = false
            }
            return
        }

        var idx = mediaList.indexAt(10, dropCoordY + contentY)
        if (idx != -1) {
            var y = mediaList.mapToItem(mediaList.itemAtIndex(idx), 10, dropCoordY).y
            if (y > itemRowHeight/2 && idx < (mediaList.count-1)) {
                idx = idx+1
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
