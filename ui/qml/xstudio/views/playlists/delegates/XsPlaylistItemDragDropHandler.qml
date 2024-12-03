// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QuickFuture 1.0
import QtQml.Models 2.15

import xstudio.qml.helpers 1.0
import xStudio 1.0

XsDragDropHandler {

    id: drag_drop_handler
    targetWidget: bgDiv

    dragSourceName: "PlayList"
    dragData: sessionSelectionModel.selectedIndexes

    onDragged: {
        if (source == "PlayList") {
            if (modelIndex.row == (modelIndex.model.rowCount(modelIndex.parent)-1) && mousePosition.y > itemRowStdHeight/2) {
                dragToEnd = true
            } else {
                dragToEnd = false
            }
        }
    }

    onDragEntered: {

        incomingDragSource = source

        if (source == "MediaList") {

            // User is dragging selected media from a MediaList into
            // an item in the Playlists panel

            // check if the user is dragging from a playlist or playlist
            // child into the same child or the parent playlist
            // (note, the parent of the items in data will be 'MediaList'.
            // The parent of the MediaList is the Subset, Timeline, Playlist.
            // If the media is from a subset, say, it's parent's parent's
            // parent's parent will be the Playlist ... we also check for an
            // attempt to drop from subset into parent playlist! - these
            // wil be no-ops)

            if (data.length &&
                (data[0].parent.parent == modelIndex ||
                    data[0].parent.parent.parent.parent == modelIndex)) {

                canReceiveDrag = false

            } else {

                canReceiveDrag = true
            }
        } else if (source == "External") {

            canReceiveDrag = true

        } else if (source == "PlayList") {

            if (isSelected) {
                canReceiveDrag = false
            } else {
                // if the selected playlists/subsets aren't SIBLINGS of 
                // this item then this item cannot be a valid target
                // for playlists reordering
                for (var i in sessionSelectionModel.selectedIndexes) {
                    var idx = sessionSelectionModel.selectedIndexes[i]
                    if (idx.parent != modelIndex.parent) {
                        canReceiveDrag = false
                        return
                    }
                }
                canReceiveDrag = true
    
            }
        }
    }

    function doMove(button, data) {
        if (button != "Cancel") {
            let type = modelIndex.model.get(modelIndex, "typeRole")

            if (button == "Move" && data.length) {
                // as we are moving out of current playlist, we need to modify
                // selection so that something other than what's being moved
                // is in the selection
                var rc = theSessionData.rowCount(data[0].parent)
                var best_idx
                for (var i = 0; i < rc; ++i) {
                    var idx = theSessionData.index(i, 0, data[0].parent)
                    if (!data.includes(idx)) {
                        best_idx = idx
                    } else if (best_idx) {
                        break
                    }
                }
                mediaSelectionModel.select(
                    helpers.createItemSelection([best_idx]),
                    ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)

            }

            let new_indexes = theSessionData.moveRows(
                data,
                -1, // insertion row: make invalid so always inserts on the end
                modelIndex,
                button == "Copy"
            )

            if(type == "Timeline" && new_indexes.length) {
                // insert new media as new tracks.
                // find stack..

                let tindex = theSessionData.index(2, 0, modelIndex)
                theSessionData.fetchMoreWait(tindex)
                let sindex = theSessionData.index(0, 0, tindex)

                let newvindex = theSessionData.insertRowsSync(0, 1, "Video Track", "Dropped", sindex)[0];
                let newaindex = theSessionData.insertRowsSync(theSessionData.rowCount(sindex), 1 ,"Audio Track", "Dropped", sindex)[0];

                for(let i = 0; i < new_indexes.length;i++) {
                    theSessionData.insertTimelineClip(i, newvindex, new_indexes[i], "")
                    theSessionData.insertTimelineClip(i, newaindex, new_indexes[i], "")
                }
            }
        }
    }

    onDropped: {

        if (!isDragTarget) return
        isDragTarget = false
        if (source == "MediaList") {
            if (data[0].parent.parent == modelIndex.parent.parent) {
                // here the source media is from the same playlist as the
                // parent playlist (i.e. copy from parent playlist into a
                // child (subset/timeline) of the same playlist)
                doMove("Copy", data)
            } else {
                dialogHelpers.multiChoiceDialog(
                    doMove,
                    "Copy/Move Media",
                    "Do you want to move or copy the media?",
                    ["Cancel", "Copy", "Move"],
                    data)
            }
        } else if (source == "External URIS") {
            Future.promise(
                theSessionData.handleDropFuture(Qt.CopyAction, {"text/uri-list": data}, modelIndex)
            ).then(function(quuids){
                mediaSelectionModel.selectNewMedia(modelIndex, quuids)
            })
        } else if (source == "External JSON") {

            Future.promise(
                theSessionData.handleDropFuture(
                    Qt.CopyAction,
                    data,
                    modelIndex)
            ).then(function(quuids){
                mediaSelectionModel.selectNewMedia(modelIndex, quuids)
            })
            return

        } else if (source == "PlayList" && canReceiveDrag) {
            var ids = []
            for (var i in data) {
                ids.push(theSessionData.get(data[i], "idRole"))
            }
            Future.promise(
                theSessionData.handleDropFuture(
                    Qt.MoveAction,
                    {"xstudio/container-ids": ids},
                    dragToEnd ? theSessionData.index(-1, -1): modelIndex)
            ).then(function(quuids){
                //console.log("quuids", quuids)
            })

        }
    }

}
