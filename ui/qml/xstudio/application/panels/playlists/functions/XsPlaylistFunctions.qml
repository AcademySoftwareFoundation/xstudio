import QtQuick
import QtQml.Models 2.15

import xStudio 1.0
import xstudio.qml.clipboard 1.0

Item {
    XsPreference {
        id: sessionRate
        path: "/core/session/media_rate"
    }

    Clipboard {
      id: clipboard
    }


    function yyyymmdd(separator="", date=new Date()) {
        return String(date.getFullYear()) + separator + String(date.getMonth()+1).padStart(2, '0') + separator + String(date.getUTCDate()).padStart(2, '0')
    }

    function addDatedDivider() {
        theSessionData.createDivider(yyyymmdd("-"))
    }

    function addPlaylist(new_name, button) {
        if (button == "Add") {
            theSessionData.createPlaylist(new_name)
        }
    }

    function addSubset(new_name, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "Subset")
        }
    }

    function addContactSheet(new_name, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "ContactSheet")
        }
    }

    function addTimeline(new_name, fps, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "Timeline", fps)
        }
    }

    function addDivider(new_name, button) {
        if (button == "Add") {
            theSessionData.createDivider(new_name)
        }
    }

    function renamePlaylist() {
        if(sessionSelectionModel.selectedIndexes.length != 1) {
            dialogHelpers.errorDialogFunc(
                "Rename Playlist ...",
                "Please select a single item to rename."
                )
        } else {
            let index = sessionSelectionModel.selectedIndexes[0]
            let name = theSessionData.get(index, "nameRole")
            let type = theSessionData.get(index, "typeRole")
            dialogHelpers.textInputDialog(
                function(new_name, button) {
			        if (button == "Cancel") return
			        theSessionData.set(index, new_name, "nameRole")
				},
                "Rename " + type,
                "Enter a new name for the " + type,
                name,
                ["Cancel", "Rename " + type])
        }
    }

    function removeUnusedMedia() {
        for (var idx = 0; idx < sessionSelectionModel.selectedIndexes.length; ++idx) {
            if (theSessionData.get(sessionSelectionModel.selectedIndexes[idx], "typeRole") == "Playlist") {
                theSessionData.purgePlaylist(sessionSelectionModel.selectedIndexes[idx])
            }
        }
    }

    function duplicate() {
        for (var i = 0; i < sessionSelectionModel.selectedIndexes.length; ++i) {
            let index = sessionSelectionModel.selectedIndexes[i]
            theSessionData.duplicateRows(index.row, 1, index.parent)
        }
    }

    function combine() {
        if(sessionSelectionModel.selectedIndexes.length) {
            theSessionData.mergeRows(sessionSelectionModel.selectedIndexes)
        }
    }

    function copySelectedNames() {
        let result = []
        for(let i =0;i<sessionSelectionModel.selectedIndexes.length;i++) {
            result.push(theSessionData.get(sessionSelectionModel.selectedIndexes[i], "nameRole"))
        }
        clipboard.text = result.join("\n")
    }

    function addPlaylistChoice() {
        dialogHelpers.textInputDialog(
            addPlaylist,
            "Add Playlist",
            "Enter a name for the new playlist.",
            theSessionData.getNextName("Playlist {}"),
            ["Cancel", "Add"])
    }

    function addSubsetChoice() {
        dialogHelpers.textInputDialog(
            addSubset,
            "Add Subset",
            "Enter a name for the new subset.",
            theSessionData.getNextName("Subset {}"),
            ["Cancel", "Add"])
    }

    function addSequenceChoice() {
        dialogHelpers.sequenceInputDialog(
            addTimeline,
            "Add Sequence",
            "New Sequence",
            theSessionData.getNextName("Sequence {}"),
            ["Cancel", "Add"],
            sessionRate.value)
    }

    function addContactSheetChoice() {
        dialogHelpers.textInputDialog(
            addContactSheet,
            "Add Contact Sheet",
            "Enter a name for the new contact sheet.",
            theSessionData.getNextName("Contact Sheet {}"),
            ["Cancel", "Add"])
    }

    function addDividerChoice() {
        dialogHelpers.textInputDialog(
            addDivider,
            "Add Playlist Divider",
            "Enter a name for the new divider.",
            theSessionData.getNextName("Divider {}"),
            ["Cancel", "Add"])
    }
}