import QtQuick 2.15

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

/* This model gives us access to the data of media in a playlist, subset, timeline 
etc. that we can iterate over with a Repeater, ListView etc. */
DelegateModel {           

    id: mediaList

    // our model is the main sessionData instance
    model: theSessionData

    // we listen to the main selection model that selects stuff in the
    // main sessionData - this thing decides which playlist, subset, timeline
    // etc. is selected to be displayed in our media list
    property var currentSelectedPlaylistIndex: sessionSelectionModel.currentIndex
    onCurrentSelectedPlaylistIndexChanged : {
        updateMedia()
    }
    
    function updateMedia() {
        if(currentSelectedPlaylistIndex.valid) {
            // wait for valid index..
            let mind = currentSelectedPlaylistIndex.model.index(0, 0, currentSelectedPlaylistIndex)
            if(mind.valid) {
                mediaList.rootIndex = mind
            } else {
                // try again in 200 milliseconds
                callback_timer.setTimeout(function() { return function() {
                    updateMedia()
                }}(), 200);
            }
        } else {
            mediaList.rootIndex = null
        }
    }
    
}

