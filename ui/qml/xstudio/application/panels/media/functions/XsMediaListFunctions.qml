// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQml.Models 2.15
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0

import xStudio 1.0

Item {
    XsPreference {
        id: sessionRate
        path: "/core/session/media_rate"
    }

    XsPreference {
        id: chunkSize
        path: "/ui/qml/contactsheet_chunk_size"
    }

    Clipboard {
      id: clipboard
    }


    function addtoNewPlaylistContext() {
        dialogHelpers.textInputDialog(
            function(name, button) {
                if(button == "Move Media") {
                    let pl = theSessionData.createPlaylist(name)
                    theSessionData.moveRows(mediaSelectionModel.selectedIndexes, 0, pl)
                } else if(button == "Copy Media") {
                    let pl = theSessionData.createPlaylist(name)
                    theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, pl)
                }
            },
            "Add To New Playlist",
            "Enter New Playlist Name",
            theSessionData.getNextName("Playlist {}"),
            ["Cancel", "Move Media", "Copy Media"])
    }


    function duplicate() {
        let items = []

        for(let i=0;i<mediaSelectionModel.selectedIndexes.length;++i)
            items.push(helpers.makePersistent(mediaSelectionModel.selectedIndexes[i]))

        items.forEach(
            (item) => {
                item.model.duplicateRows(item.row, 1, item.parent)
            }
        )
    }

    function relinkMedia(loose=false) {
       dialogHelpers.showFolderDialog(
            function(path, chaserFunc) {
                if(path)
                    theSessionData.relinkMedia(mediaSelectionModel.selectedIndexes, path, loose)
            },
            file_functions.defaultSessionFolder(),
            "Relink media files")
    }


    function setMediaRotation(rotation, button) {
        if (button == "Cancel") return
        var rot = typeof rotation === "string" ? parseFloat(rotation) : rotation
        mediaSelectionModel.selectedIndexes.forEach(
            (element) => {
                element.model.set(element, rot, "rotationRole")
            }
        )
    }

    function copyQuickViewToClipboard() {
        let filenames = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")

        for(let i =0;i<filenames.length;i++) {
            filenames[i] = helpers.pathFromURL(filenames[i])
        }

        clipboard.text = "xstudio -l '" + filenames.join("' '") +"'"
    }

    function copyFilePathsToClipboard() {
        let result = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")
        for(let i =0;i<result.length;i++) {
            result[i] = helpers.pathFromURL(result[i])
        }

        clipboard.text = result.join("\n")
    }

    function copyEmailLinkToClipboard() {
        let name = encodeURIComponent(inspectedMediaSetProperties.values.nameRole)
        let prefix = "&" + name + "_media="
        let filenames = mediaSelectionModel.getSelectedMediaUrl()
        for(let i =0;i<filenames.length;i++) {
            filenames[i] = helpers.pathFromURL(filenames[i])
        }

        clipboard.text = sessionLinkPrefix.value + "xstudio://add_media?compare="+
            encodeURIComponent(currentPlayhead.compareMode)+"&playlist=" +
            name + prefix +
            filenames.join(prefix)
    }

    function setMediaAspect() {
        let mi = mediaSelectionModel.selectedIndexes[0]
        let ms = theSessionData.searchRecursive(theSessionData.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)

        dialogHelpers.numberInputDialog(
            function(new_pixel_aspect, button) {
                if(button == "Set Pixel Aspect") {
                    mediaSelectionModel.selectedIndexes.forEach(
                        (element) => {
                            let mi = theSessionData.searchRecursive(theSessionData.get(element, "imageActorUuidRole"), "actorUuidRole", element)
                            mi.model.set(mi, parseFloat(new_pixel_aspect), "pixelAspectRole")
                        }
                    )
                }
            },
            "Set Pixel Aspect Ratio",
            "Enter new pixel aspect ratio to apply to selected media",
             mi.model.get(ms, "pixelAspectRole"),
            ["Cancel", "Set Pixel Aspect"])
    }

    function setMediaFPS() {
        let mi = mediaSelectionModel.selectedIndexes[0]
        let ms = theSessionData.searchRecursive(theSessionData.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)

        dialogHelpers.numberInputDialog(
            function(new_rate, button) {
                if(button == "Set Rate") {
                    mediaSelectionModel.selectedIndexes.forEach(
                        (element) => {
                            let mi = theSessionData.searchRecursive(theSessionData.get(element, "imageActorUuidRole"), "actorUuidRole", element)
                            mi.model.set(mi, new_rate, "rateFPSRole")
                        }
                    )
                }
            },
            "Set Media Rate",
            "Enter New Media Rate",
             mi.model.get(ms, "rateFPSRole"),
            ["Cancel", "Set Rate"])
    }

    function openInQuickView() {
        if (mediaSelectionModel.selectedIndexes.length > 8) {
            dialogHelpers.errorDialogFunc(
                "Warning",
                "The quick view feature is limited to a selection of 8 or fewer media items!"
                )
            return;
        }
        for (var i in mediaSelectionModel.selectedIndexes) {
            let idx = mediaSelectionModel.selectedIndexes[i]
            var actor = theSessionData.get(idx, "actorRole")
            quick_view_launcher.launchQuickViewer([actor], "Off", -1, -1)
        }
    }

    function copyFileNamesToClipboard() {
        let result = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")
        for(let i =0;i<result.length;i++) {
            result[i] = helpers.pathFromURL(result[i])
            result[i] = result[i].substr(result[i].lastIndexOf("/")+1)
        }
        clipboard.text = result.join("\n")
    }

    function addToNewPlaylistDialog() {
        dialogHelpers.textInputDialog(
            function(new_name, button) {
                if (button == "Add") {
                   let pl = theSessionData.createPlaylist(new_name)
                    theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, pl)
                }
            },
            "Add Playlist",
            "Enter a name for the new playlist.",
            theSessionData.getNextName("Playlist {}"),
            ["Cancel", "Add"])
    }

    function addToNewSubsetDialog() {
        dialogHelpers.textInputDialog(
            function(new_name, button) {
                if (button == "Add") {
                    addToNewSubset(new_name)
                }
            },
            "Add Subset",
            "Enter a name for the new subset.",
            theSessionData.getNextName("Subset {}"),
            ["Cancel", "Add"])
    }

    function addToNewContactSheetDialog() {
        dialogHelpers.textInputDialog(
            function(new_name, button) {
                if (button == "Add") {
                    addToNewContactSheet(new_name)
                }
            },
            "Add Contact Sheet",
            "Enter New Contact Sheet Name.",
            theSessionData.getNextName("Contact Sheet {}"),
            ["Cancel", "Add"])
    }

    function addToNewContactSheetsDialog() {
        dialogHelpers.contactSheetDialog(
            function(name, count, button) {
                if(button == "Add") {
                    let icount = parseInt(count);
                    if(chunkSize.value != icount)
                        chunkSize.value = icount;

                    addToNewContactSheet(name, icount);
                }
            },
            "Add Contact Sheets",
            "Enter New Contact Sheets Name",
            "Contact Sheet {}",
            chunkSize.value,
            ["Cancel", "Add"])
    }

    function addToNewSequenceDialog() {
        let rate = sessionRate.value

        if(mediaSelectionModel.selectedIndexes.length) {
            let actoruuid = theSessionData.get(mediaSelectionModel.selectedIndexes[0], "imageActorUuidRole")
            let image_source = theSessionData.searchRecursive(actoruuid, "actorUuidRole", mediaSelectionModel.selectedIndexes[0])
            if (image_source.valid) {
                rate = theSessionData.get(image_source, "rateFPSStringRole")
            }
        }
        dialogHelpers.sequenceInputDialog(
            function(new_name, fps, button) {
                if (button == "Add") {
                    addToNewSequence(new_name, fps)
                }
            },
            "Add Sequence",
            "New Sequence",
            theSessionData.getNextName("Sequence {}"),
            ["Cancel", "Add"],
            rate)
    }

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


    function cycleColour(indexes) {
        let colours = [
            "#FFFF0000",
            "#FF00FF00",
            "#FF0000FF",
            "#FFFFFF00",
            "#FFFFA500",
            "#FF800080",
            "#FF000000",
            "#FFFFFFFF",
            "#00000000",
        ]

        indexes.forEach(
            function (item, index) {
                let current = theSessionData.get(item, "flagColourRole")
                let cind = colours.indexOf(current)
                if(cind == -1 || cind == colours.length-1)
                    theSessionData.set(item, colours[0], "flagColourRole")
                else
                    theSessionData.set(item, colours[cind+1], "flagColourRole")
            }
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
        let items = [].concat(mediaSelectionModel.selectedIndexes)

        if(force)
            deleteSelectedCallback("Delete", items)
        else {
            dialogHelpers.multiChoiceDialog(
                function(response) {deleteSelectedCallback(response, items)},
                "Delete Media",
                "Remove the selected media?",
                ["Cancel", "Delete"],
                undefined)
        }
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
        if(indexes.length) {
            let items = [].concat(indexes)
            items = items.sort((a,b) => b.row - a.row )

            if(mediaSelectionModel.selectedIndexes.toString() === indexes.toString()) {
                var onscreen_idx = mediaIndexAfterRemoved(items)
                mediaSelectionModel.setCurrentIndex(onscreen_idx, ItemSelectionModel.setCurrentIndex)
                mediaSelectionModel.select(onscreen_idx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
            }

            items.forEach(function (item, index) {
                item.model.removeRows(item.row, 1, false, item.parent)
            })
        }
    }

    function deleteSelectedCallback(response, indexes) {
        if (response == "Delete")
            deleteIndexes(indexes)
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

    function addToNewSequence(name, rate) {
        let sub = theSessionData.createPlaylistChild(name, "Timeline", rate, theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))

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
        let sub = theSessionData.createPlaylistChild(name, "Subset", "", theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))
        theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, sub)
    }


    function addToNewContactSheet(name, count = -1) {
        if(count == -1) {
            let sub = theSessionData.createPlaylistChild(name, "ContactSheet", "", theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]))
            theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, sub)
        } else {
            function splitArray(arr, chunkSize) {
                return Array.from({ length: Math.ceil(arr.length / chunkSize) }, (_, i) =>
                    arr.slice(i * chunkSize, i * chunkSize + chunkSize)
                );
            }

            let chunks = splitArray(mediaSelectionModel.selectedIndexes, count);
            let parent = theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0])
            for(let i = 0; i < chunks.length; i++) {
                let sub = theSessionData.createPlaylistChild(theSessionData.getNextName(name), "ContactSheet", "", parent)
                theSessionData.copyRows(chunks[i], 0, sub)
            }
        }
    }
}