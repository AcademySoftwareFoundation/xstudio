import QtQuick 2.15

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import xStudio 1.0

/* This model gives us access to the data of media in a playlist, subset, timeline
etc. that we can iterate over with a Repeater, ListView etc. */
Item {

    id: mediaList

    // This gives us a model that just contains a flat list of the
    // Media items in the current media set (Playlist, Sequence, Subset) -
    // it is a  the node in the sessionData at index = currentMediaContainerIndex
    QTreeModelToTableModel {
        id: mediaListModelData
        model: theSessionData
    }

    // This filters the media items by inspecting the mediaDisplayInfoRole which
    // contains the

    property alias model: filteredModel
    property var rootIndex

    XsMediaListFilterModel {
        sourceModel: mediaListModelData
        searchString: mediaListSearchString
        id: filteredModel
    }

    // we listen to the main selection model that selects stuff in the
    // main sessionData - this thing decides which playlist, subset, timeline
    // etc. is selected to be displayed in our media list
    property var currentSelectedPlaylistIndex: currentMediaContainerIndex
    property var name: theSessionData.get(currentSelectedPlaylistIndex, "nameRole")
    property var uuid: theSessionData.get(currentSelectedPlaylistIndex, "uuidRole")

    onCurrentSelectedPlaylistIndexChanged : updateMedia(true)

    function updateMedia(retry) {
        if(currentSelectedPlaylistIndex.valid) {
            // wait for valid index..
            let mind = currentSelectedPlaylistIndex.model.index(0, 0, currentSelectedPlaylistIndex)

            if(mind.valid) {
                mediaListModelData.rootIndex = helpers.makePersistent(currentSelectedPlaylistIndex)
                mediaListModelData.expandRow(0)
                mediaList.rootIndex = mind
            } else if (retry) {
                // try again in 200 milliseconds
                callbackTimer.setTimeout(function() { return function() {
                    updateMedia(false)
                }}(), 200);
            }
        } else {
            mediaListModelData.rootIndex = theSessionData.index(-1,-1)
        }
    }


}

