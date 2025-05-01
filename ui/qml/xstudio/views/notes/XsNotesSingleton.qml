// SPDX-License-Identifier: Apache-2.0

import QtQuick
import xstudio.qml.viewport 1.0

import xStudio 1.0

Item {

    XsPreference {
        id: note_category
        index: globalStoreModel.searchRecursive("/core/bookmark/note_category", "pathRole")
    }
    property alias note_category: note_category.value

    XsPreference {
        id: note_colour
        index: globalStoreModel.searchRecursive("/core/bookmark/note_colour", "pathRole")
    }
    property alias note_colour: note_colour.value

    XsHotkey {
        sequence: ";"
        name: "Create new note"
        description: "Creates a new empty note on the current frame"
        context: "any"
        onActivated: {
            if(bookmarkModel.insertRows(bookmarkModel.rowCount(), 1)) {
                // set owner..
                var nm = currentOnScreenMediaData.values.nameRole
                if (nm == "New Media") {
                    nm = helpers.fileFromURL(currentOnScreenMediaSourceData.values.pathRole)
                }    
                // There are 705600000 flicks in one second
                var media_position_flicks = Math.round(currentPlayhead.mediaFrame*705600000.0/currentOnScreenMediaSourceData.values.rateFPSRole)

                let ind = bookmarkModel.index(bookmarkModel.rowCount()-1, 0)
                bookmarkModel.set(ind, currentOnScreenMediaData.values.actorUuidRole, "ownerRole")
                bookmarkModel.set(ind, media_position_flicks, "startRole")
                bookmarkModel.set(ind, nm, "subjectRole")
                bookmarkModel.set(ind, 0, "durationRole")
                bookmarkModel.set(ind, note_category, "categoryRole")
                bookmarkModel.set(ind, note_colour, "colourRole")
            }
        }
    }
}