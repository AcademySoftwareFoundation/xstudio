import QtQuick 2.15

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.global_store_model 1.0

import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

Item {

    id: collecion
    property var sessionActorAddr

    XsSessionModel {

        id: sessionData
        sessionActorAddr: collecion.sessionActorAddr

    }
    property alias session: sessionData


    XsGlobalStoreModel {
        id: globalStoreModel
    }
    property alias globalStoreModel: globalStoreModel

    /* selectedMediaSetIndex is the index into the model that points to the 'selected'
    playlist, subset, timeline etc. - the selected media set is the playlist, 
    subset or timeline is the last one to be single-clicked on in the playlists
    panel. The selected media set is what is shown in the media list for example
    but can and often is different to the 'viewedMediaSet'  */
    property var selectedMediaSetIndex: session.index(-1, -1)
    onSelectedMediaSetIndexChanged: {

        sessionSelectionModel.select(selectedMediaSetIndex, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
        sessionSelectionModel.setCurrentIndex(selectedMediaSetIndex, ItemSelectionModel.setCurrentIndex)

    }


    /* viewedMediaSetIndex is the index into the model that points to the 'active'
    playlist, subset, timeline etc. - the active media set is the playlist, 
    subset or timeline that is being viewed in the viewport and shows in the
    timeline panel */
    property var viewedMediaSetIndex: session.index(-1, -1)

    onViewedMediaSetIndexChanged: {

        session.setPlayheadTo(viewedMediaSetIndex)

        // get the index of the PlayheadSelection node for this playlist
        let ind = session.search_recursive("PlayheadSelection", "typeRole", viewedMediaSetIndex)

        if (ind.valid) {
            // make the 'mediaSelectionModel' track the PlayheadSelection
            playheadSelectionIndex = ind
            mediaSelectionModel.updateSelection()

        }

        // get the index of the Playhead node for this playlist
        let ind2 = session.search_recursive("Playhead", "typeRole", viewedMediaSetIndex)
        if (ind2.valid) {
            // make the 'mediaSelectionModel' track the PlayheadSelection
            currentPlayheadProperties.index = ind2
        }

    }

    // This ItemSelectionModel manages playlist, subset, timeline etc. selection
    // from the top-level session. Of the selection, the first selected item
    // is the 'active' playlist/subset/timeline that is shown in the medialist
    // and viewport
    ItemSelectionModel {
        id: sessionSelectionModel
        model: sessionData
    }
    property alias sessionSelectionModel: sessionSelectionModel

    /* playheadSelectionIndex is the index into the model that points to the 'active'
    playheadSelectionActor - Each playlist, subset, timeline has its own
    playheadSelectionActor and this is the object that selectes media from the
    playlist to be shown in the viewport (and compared with A/B, String compare
    modes etc.) */
    property var playheadSelectionIndex

    /* Here we use XsModelPropertyMap to track the Uuid of the 'current' playhead.
    Note that we set the index for this in onviewedMediaSetIndexChanged above */
    XsModelPropertyMap {
        id: currentPlayheadProperties
        property var playheadUuid: values.actorUuidRole
    }

    /* This XsModuleData talks to a backend data model that contains all the
    attribute data of the Playhead object and exposes it as data in QML as
    a QAbstractItemModel. Every playhead instance in the app publishes its own
    data model which is identified by the uuid of the playhead - by changing the
    'modelDataName' to the Uuid of the current playhead we get access to the
    data of the current active playhead.

    If this seems confusing it's because it is! We have two different ways of
    exposing the data of backend objects - the main Session model and then more
    flexible 'XsModuleData' that can be set-up (in the backend) to include some
    or all of the data from one or several backend objects.

    At some point we may rationalise this and build into the singe Session model*/
    XsModuleData {

        id: current_playhead_data

        // this is how we link up with the backend data model that gives
        // access to all the playhead attributes data
        modelDataName: currentPlayheadProperties.playheadUuid ? currentPlayheadProperties.playheadUuid : ""
    }
    property alias current_playhead_data: current_playhead_data

    // This ItemSelectionModel manages media selection within the current
    // active playlist/subset/timeline etc.
    ItemSelectionModel {

        id: mediaSelectionModel
        model: session

        onSelectionChanged: {
            if(selectedIndexes.length) {
                session.updateSelection(playheadSelectionIndex, selectedIndexes)
            }
        }

        // This is pretty baffling..... Shouldn't the backend playhead
        // selectin actor update the model for us instead of this gubbins?
        function updateSelection() {

            // the playheadSelection item is a child of the playlist (or subset,
            // timeline etc) so use this to get to the playlist
            let playlistIndex = playheadSelectionIndex.parent

            // iterator over the playheadSelection rows ...
            /*let count = sessionData.rowCount(playheadSelectionIndex)
            for(let i =0; i<count;i++) {
                // TODO: actually work out how to fill out selectedIndexes
                let foo = playheadSelectionIndex.index(i, 0)
                console.log("foo", foo)
            }*/
        }
    }
    property alias mediaSelectionModel: mediaSelectionModel


    /* This model gives us access to the playlists */
    DelegateModel {

        id: playlistsModelData

        // this is required as "model" doesn't issue notifications on change
        property var notifyModel: theSessionData

        // our model is the main sessionData instance
        model: notifyModel

        // the playlists in the session start one level down at row=0, column=0. Easy!
        rootIndex: notifyModel.index(0, 0, notifyModel.index(-1,-1))

    }
    property alias playlistsModelData: playlistsModelData


    /* This gives us direct access to the properties of the current (active)
    playlist - for example viewedMediaSetProperties.values.nameRole gives us
    the playlist name */
    XsModelPropertyMap {
        id: viewedMediaSetProperties
        index: viewedMediaSetIndex
    }
    property alias viewedMediaSetProperties: viewedMediaSetProperties

    /* This gives us direct access to the properties of the SELECTED playlist,
    subset or timeline (i.e. the last one to be clicked on in the playlist
    view panel) - for example selectedMediaSetProperties.values.nameRole 
    gives us the playlist name */
    XsModelPropertyMap {
        id: selectedMediaSetProperties
        index: selectedMediaSetIndex
    }
    property alias selectedMediaSetProperties: selectedMediaSetProperties

    Timer {
        id: callbackTimer
        function setTimeout(cb, delayTime) {
            callbackTimer.interval = delayTime;
            callbackTimer.repeat = false;
            callbackTimer.triggered.connect(cb);
            callbackTimer.triggered.connect(function release () {
                callbackTimer.triggered.disconnect(cb); // This is important
                callbackTimer.triggered.disconnect(release); // This is important as well
            });
            callbackTimer.start();
        }
    }
    property alias callbackTimer: callbackTimer
        
}