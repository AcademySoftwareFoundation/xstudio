// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.15

import xStudio 1.0

Item {

    function selectAll() {
        var selection = []
        for (var i = 0; i < mediaListModelData.rowCount(); ++i) {
            selection.push(mediaListModelData.rowToSourceIndex(i))
        }

        mediaSelectionModel.select(
            helpers.createItemSelection(selection),
            ItemSelectionModel.ClearAndSelect
        )
    }

    function invertSelection() {
        var selection = []
        for (var i = 0; i < mediaListModelData.rowCount(); ++i) {
            if(! mediaSelectionModel.selectedIndexes.includes(mediaListModelData.rowToSourceIndex(i)))
                selection.push(mediaListModelData.rowToSourceIndex(i))
        }

        mediaSelectionModel.select(
            helpers.createItemSelection(selection),
            ItemSelectionModel.ClearAndSelect
        )
    }

    function getOffline() {
        var selection = []

        for (var i = 0; i < mediaListModelData.rowCount(); ++i) {
            let si = mediaListModelData.rowToSourceIndex(i)
            let state = theSessionData.get(si, "mediaStatusRole")
            if(state != undefined && state != "Online")
                selection.push(mediaListModelData.rowToSourceIndex(i))
        }

        return selection
    }

    function selectAllOffline() {
        mediaSelectionModel.select(
            helpers.createItemSelection(getOffline()),
            ItemSelectionModel.ClearAndSelect
        )
    }

    function selectAllAdjusted() {
        var selection = []
        for (var i = 0; i < mediaListModelData.rowCount(); ++i) {
            let si = mediaListModelData.rowToSourceIndex(i)
            let bk = theSessionData.get(si, "bookmarkUuidsRole")
            if(bk != undefined && bk.length)
                selection.push(mediaListModelData.rowToSourceIndex(i))
        }

        mediaSelectionModel.select(
            helpers.createItemSelection(selection),
            ItemSelectionModel.ClearAndSelect
        )
    }

    function selectFlagged(flag) {
        var selection = []
        for (var i = 0; i < mediaListModelData.rowCount(); ++i) {
            let si = mediaListModelData.rowToSourceIndex(i)
            if(theSessionData.get(si, "flagColourRole") == flag)
                selection.push(mediaListModelData.rowToSourceIndex(i))
        }

        mediaSelectionModel.select(
            helpers.createItemSelection(selection),
            ItemSelectionModel.ClearAndSelect
        )
    }


    function deselectAll() {
        mediaSelectionModel.clear()
    }

    function mediaIndexAfterRemoved(indexes) {

        let select_row = -1;
        let to_remove = []
        let parent = indexes[0].parent;

        for(let i =0; i<indexes.length; ++i)
            to_remove.push(indexes[i].row)

        to_remove = to_remove.sort(function(a,b){return a-b})

        while(select_row == -1 && to_remove.length) {
            select_row = to_remove[0] - 1
            to_remove.shift()
        }

        return parent.model.index(select_row, 0, parent)
    }

    function deleteSelected(force=false) {
        if(force)
            deleteSelectedCallback("Delete")
        else
            dialogHelpers.multiChoiceDialog(
                deleteSelectedCallback,
                "Delete Media",
                "Remove the selected media?",
                ["Cancel", "Delete"],
                undefined)

    }

    function deleteOffline(force=false) {
        if(force)
            deleteIndexes(getOffline())
        else
            dialogHelpers.multiChoiceDialog(
                deleteOfflineCallback,
                "Delete Offline Media",
                "Remove the offline media?",
                ["Cancel", "Delete"],
                undefined)

    }

    function deleteOfflineCallback(response) {
        if (response == "Delete")
            deleteIndexes(getOffline())
    }

    function deleteIndexes(indexes) {
        let items = []
        for(let i=0;i<indexes.length;++i)
            items[i] = indexes[i]
        items = items.sort((a,b) => b.row - a.row )

        var onscreen_idx = mediaIndexAfterRemoved(items)
        mediaSelectionModel.setCurrentIndex(onscreen_idx, ItemSelectionModel.setCurrentIndex)
        mediaSelectionModel.select(onscreen_idx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)

        items.forEach(function (item, index) {
            item.model.removeRows(item.row, 1, false, item.parent)
        })
    }

    function deleteSelectedCallback(response) {
        if (response == "Delete")
            deleteIndexes(mediaSelectionModel.selectedIndexes)
    }

    function selectUp() {

        if (!mediaSelectionModel.selectedIndexes.length) return;
        var first_row = 10000000
        for (let i = 0; i < mediaSelectionModel.selectedIndexes.length; i++) {
            first_row = Math.min(mediaSelectionModel.selectedIndexes[i].row, first_row)
        }
        if (first_row > 0) {
            let parent = mediaSelectionModel.selectedIndexes[0].parent;
            mediaSelectionModel.select(
                parent.model.index(first_row-1, 0, parent),
                ItemSelectionModel.Select
                )
        }
    }

    function selectDown() {

        if (!mediaSelectionModel.selectedIndexes.length) return;
        var last_row = -1000000
        for (let i = 0; i < mediaSelectionModel.selectedIndexes.length; i++) {
            last_row = Math.max(mediaSelectionModel.selectedIndexes[i].row, last_row)
        }
        let parent = mediaSelectionModel.selectedIndexes[0].parent;
        if (last_row > 0 && parent.model.rowCount(parent) > (last_row+1)) {
            mediaSelectionModel.select(
                parent.model.index(last_row+1, 0, parent),
                ItemSelectionModel.Select
                )
        }
    }

    function addToNewSequence(name) {
        let sub = theSessionData.createPlaylistChild(name, "Timeline", theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))

        // get reference to timelineitem
        let tindex = theSessionData.index(2, 0, sub)

        theSessionData.fetchMoreWait(tindex)
        let sindex = theSessionData.index(0, 0, tindex)
        let vindex = theSessionData.index(0, 0, sindex)
        let aindex = theSessionData.index(1, 0, sindex)

        // copy media into timeline medialist.
        theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, sub)

        for(let i = 0; i< mediaSelectionModel.selectedIndexes.length; i++) {
            let mindex = mediaSelectionModel.selectedIndexes[i]
            if(mindex.valid && vindex.valid && aindex.valid) {
                theSessionData.insertTimelineClip(i, vindex, mindex, "")
                theSessionData.insertTimelineClip(i, aindex, mindex, "")
            }
        }
    }

    function addToNewSubset(name) {
        let sub = theSessionData.createPlaylistChild(name, "Subset", theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))
        theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, sub)
    }

    function addToNewContactSheet(name) {
        let sub = theSessionData.createPlaylistChild(name, "ContactSheet", theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))
        theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, sub)
    }

    
}