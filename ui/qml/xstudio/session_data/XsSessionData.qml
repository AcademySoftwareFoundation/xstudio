import QtQuick 2.15

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.global_store_model 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.embedded_python 1.0

import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

Item {

    id: collecion
    property var sessionActorAddr

    XsSessionModel {

        id: sessionData
        sessionActorAddr: collecion.sessionActorAddr

        // This index points to the list of playlists within the session
        property var playlistsRootIdx: helpers.makePersistent(index(0, 0, index(-1, -1)))

        onSessionActorAddrChanged: {
            // a new session has been created/loaded. Pick the first playlist
            // after 200ms to give the session data a chance to build itself
            callbackTimer.setTimeout(function() { return function() {
                mediaSelectionModel.select(
                    helpers.createItemSelection([session.searchRecursive("Playlist", "typeRole")]),
                    ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
                }}(), 200);
        }

        // Not sure if we need to update the playlists root idx on ALL of these
        // signals. One thing we can't do is automatically maintain a 'static'
        // index without triggering off these signals or using a DelegateModel
        onModelReset: {
            playlistsRootIdx = helpers.makePersistent(index(0, 0, index(-1, -1)))
        }
        onCountChanged: {
            playlistsRootIdx = helpers.makePersistent(index(0, 0, index(-1, -1)))
        }

        function checkCurrentMediaContainer(retry) {

            if(currentMediaContainerIndex.valid) {
                // wait for valid index..
                sessionSelectionModel.setCurrentIndex(currentMediaContainerIndex, ItemSelectionModel.ClearAndSelect)

            } else if (retry) {
                updateCurrentMediaContainerIndexFromBackend()
                updateViewportCurrentMediaContainerIndexFromBackend()
                // backend actor may have told us the current playlist has
                // changed, but the playlist hasn't been added to the UI model
                // yet. Check back in 200 ms.
                callbackTimer.setTimeout(function() { return function() {
                    checkCurrentMediaContainer(true)
                }}(), 200);
            } else {
                // clear the selection
                // sessionSelectionModel.clear()
            }
        }

        onCurrentMediaContainerChanged: {

            checkCurrentMediaContainer(true);

            // get the index of the PlayheadSelection node for the inspected playlist
            let ind = session.searchRecursive("PlayheadSelection", "typeRole", currentMediaContainerIndex)

            if (ind.valid) {
                // make the 'mediaSelectionModel' track the PlayheadSelection
                playheadSelectionIndex = helpers.makePersistent(ind)

            }

            // if currentMediaContainerIndex is a Subset or Timeline, get
            // it's parent playlist
            let pl = session.getPlaylistIndex(currentMediaContainerIndex)

            // if indeed currentMediaContainerIndex is a Subset or Timeline
            // we want to ensure that the parent playist is expanded
            if(pl != currentMediaContainerIndex)
                session.set(pl, true, "expandedRole")

        }

        function createPlaylist(name, sync=true) {

            // always add at end of playlists list
            var insert_row = rowCount(index(0,0));

            var idx = insertRowsSync(
                insert_row,
                1,
                "Playlist",
                name,
                index(0,0)
            )[0]

            sessionSelectionModel.setCurrentIndex(
                idx,
                ItemSelectionModel.ClearAndSelect)

            return idx
        }

        function createPlaylistChild(name, type, playlistIndex, insert_before) {
            // note row 2 of Playlist in tree is the list of subsets, contactsheets etc

            var newidx = sessionData.insertRowsSync(
                insert_before==undefined ? sessionData.rowCount(sessionData.index(2, 0, playlistIndex)) : insert_before,
                1, type, name,
                sessionData.index(2,0, playlistIndex)
            )

            // hideous!!
            callbackTimer.setTimeout(function(index) { return function() {
                sessionData.fetchMore(index)
                sessionData.fetchMore(index.parent)
                sessionSelectionModel.setCurrentIndex(
                    index,
                    ItemSelectionModel.ClearAndSelect)

                }}( newidx[0] ), 200);
            return newidx[0];
        }

        function createSubItem(name, type) {
            if (currentMediaContainerIndex.valid) {

                var selectedType = sessionData.get(currentMediaContainerIndex, "typeRole")
                var result
                if (selectedType== "Playlist") {
                    // A playlist is selected. Add as a child.
                    result = createPlaylistChild(name, type, currentMediaContainerIndex)
                } else if (["Subset", "Timeline", "ContactSheet"].includes(type)) {
                    // Subset or similar is selected. Insert after
                    result = createPlaylistChild(
                        name,
                        type,
                        currentMediaContainerIndex.parent.parent,
                        currentMediaContainerIndex.row+1
                        )

                } else {
                    // Not sure what's selected. try to find playlist?
                    var p = currentMediaContainerIndex
                    while (p.valid) {
                        if (p.model.get(p, "typeRole") == "Playlist") {
                            result = createPlaylistChild(name, type, p)
                            break
                        }
                        p = p.parent
                    }
                }

                if (result && result.valid) {
                    theSessionData.set(result.parent.parent, true, "expandedRole")
                }
                return result

            }
            // can't find a selected playlist to add to. Need a new playlist
            var p = createPlaylist("New Playlist")
            theSessionData.set(p, true, "expandedRole")
            // need a delay to allow the playlist node in the model to be
            // filled out by the backend
            callbackTimer.setTimeout(function(name, type, p) { return function() {
                createPlaylistChild(name, type, p)
                }}( name, type, p ), 200);
        }

        function createDivider(name) {
            if (currentMediaContainerIndex.valid) {
                sessionData.insertRowsSync(
                    currentMediaContainerIndex.row+1,
                    1, "ContainerDivider", name,
                    currentMediaContainerIndex.parent
                    )
            } else {
                sessionData.insertRowsSync(
                    0,
                    1, "ContainerDivider", name,
                    playlistsRootIdx
                    )
            }
        }

        // This function will select the parent playlist and the media specified
        // so that it is viewed in the viewport - i.e. it becomes the onScreenMedia
        // item
        function putMediaOnScreen(media_uuid) {

            // search current viewed playlist first ...
            var idx = session.index(-1, -1)
            if (theSessionData.get(sessionData.viewportCurrentMediaContainerIndex, "typeRole") != "Timeline") {
                idx = session.searchRecursive(media_uuid, "actorUuidRole", sessionData.viewportCurrentMediaContainerIndex)
            }

            if (!idx.valid) {
                // search playlists only to find the media we want to put on
                // the screen, because we want to avoid searching timelines
                var playlists = session.searchRecursiveList("Playlist", "typeRole", session.index(-1,-1), 0, -1)
                for (var i = 0; i < playlists.length; ++i) {
                    // 'depth' of 2 here means we will search media in the playlist but not media in
                    // a timeline that is inside this playlist
                    idx = session.searchRecursive(media_uuid, "actorUuidRole", playlists[i], 0, 2)
                    if (idx.valid) break
                }

                if (!idx.valid) {
                    // last shot, search the whole session
                    idx = session.searchRecursive(media_uuid, "actorUuidRole")
                }
            }
            if (idx.valid) {
                mediaSelectionModel.select(
                    helpers.createItemSelection([idx]),
                    ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex
                    )
            }
        }
    }

    property alias currentMediaContainerIndex: sessionData.currentMediaContainerIndex
    property alias viewportCurrentMediaContainerIndex: sessionData.viewportCurrentMediaContainerIndex

    EmbeddedPython {
        id: embeddedPython
        property string text: ""
        Component.onCompleted: {
            embeddedPython.createSession()
        }

        onStdoutEvent: text += str
        onStderrEvent: text += str
    }

    property alias embeddedPython: embeddedPython

    property alias session: sessionData

    /* This gives us access to a few properties like the filesystem path of
    the current session:
    sessionProperties.values.pathRole
    */
    XsModelPropertyMap {
        id: sessionProperties
        index: sessionData.playlistsRootIdx
    }
    property alias sessionProperties: sessionProperties

    XsGlobalStoreModel {
        id: globalStoreModel
    }
    property alias globalStoreModel: globalStoreModel

    // This ItemSelectionModel manages playlist, subset, timeline etc. selection
    // from the top-level session. Of the selection, the first selected item
    // is the 'active' playlist/subset/timeline that is shown in the medialist
    // and viewport
    ItemSelectionModel {
        id: sessionSelectionModel
        model: sessionData
        onCurrentIndexChanged: {
            currentMediaContainerIndex = currentIndex
        }
    }
    property alias sessionSelectionModel: sessionSelectionModel

    /* playheadSelectionIndex is the index into the model that points to the 'active'
    playheadSelectionActor - Each playlist, subset, timeline has its own
    playheadSelectionActor and this is the object that selectes media from the
    playlist to be shown in the viewport (and compared with A/B, String compare
    modes etc.) */
    property var playheadSelectionIndex: session.index(-1, -1)
    XsModelProperty {
        id: __selectedMedia
        index: playheadSelectionIndex
        role: "selectionRole"
    }
    property var playlistSelectedMediaActors: __selectedMedia.value
    onPlaylistSelectedMediaActorsChanged: {
        if (playlistSelectedMediaActors != undefined) {
            mediaSelectionModel.updateSelection(playlistSelectedMediaActors)
        }
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
    XsPlayhead {
        id: current_playhead
        uuid: sessionData.onScreenPlayheadUuid
        onMediaUuidChanged: {
            current_onscreen_media_data.index = session.searchRecursive(mediaUuid, "actorUuidRole", sessionData.viewportCurrentMediaContainerIndex)
        }
    }
    property alias current_playhead: current_playhead

    XsModelPropertyMap {
        id: current_onscreen_media_data
        property var imageActorUuid: values.imageActorUuidRole ? values.imageActorUuidRole : undefined
        onImageActorUuidChanged: {
            current_onscreen_media_source_data.index = session.searchRecursive(imageActorUuid, "actorUuidRole", index)
            if (!current_onscreen_media_source_data.index.valid) {
                // oh dear ... model data behind sessionData hasn't been filled
                // in for the media sources of the on-screen media item.
                // Let's fetch them
                session.fetchMore(index)
                callbackTimer.setTimeout(function(idx) { return function() {
                    current_onscreen_media_source_data.index = session.searchRecursive(imageActorUuid, "actorUuidRole", idx)
                    }}(index), 100);
            }
        }
    }
    property alias currentOnScreenMediaData: current_onscreen_media_data

    XsModelPropertyMap {
        id: current_onscreen_media_source_data
    }
    property alias currentOnScreenMediaSourceData: current_onscreen_media_source_data


    // This ItemSelectionModel manages media selection within the current
    // active playlist/subset/timeline etc.
    ItemSelectionModel {

        id: mediaSelectionModel
        model: session

        property bool updateBackend: true
        property var lastContainerWithUserSelection

        onSelectionChanged: {
            session.setSelectedMedia(selectedIndexes)
            // auto populate media sources.
            selectedIndexes.forEach(function (item, index) {
                 if(session.canFetchMore(item))
                    session.fetchMore(item)
            })

            if (updateBackend) {
                lastContainerWithUserSelection = currentMediaContainerIndex
                session.updateSelection(playheadSelectionIndex, selectedIndexes)
            }
        }

        property bool multiSelected: selection.length > 1

        // this method is called by some functions that load media into a playlist.
        // The idea is that when the loading is complete, we want to select the
        // first item in the playlist for display. However .... what happens if
        // the user has already selected something before the playlist has
        // finished building? In that case, we don't want to override their
        // selection.
        // As such we make a note of the mediacontainer index when the user does
        // a selection... if the index hasn't changed since then we don't
        // so anything here.
        function selectFirstNewMedia(index, quuids) {
            if (lastContainerWithUserSelection != currentMediaContainerIndex) {
                selectNewMedia(index, [quuids[0]])
            }
        }

        function selectNewMedia(index, quuids, playhead_index=-1, mode=ItemSelectionModel.ClearAndSelect) {

            // see note above, we don't want to change the selection automatically if
            // the user has made their own selection
            if (lastContainerWithUserSelection != currentMediaContainerIndex) return;

            let type = index.model.get(index,"typeRole")
            if(quuids.length && ["Playlist", "Subset", "Timeline", "ContactSheet"].includes(type)) {
                // selects the playlist (or subset or timeline) corresponding to 'index'
                mediaSelectionModel.select(
                    helpers.createItemSelection([index]),
                    ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
                callbackTimer.setTimeout(function(plindex, new_items) {
                    return function() {
                        let indexes = []

                        for(let i = 0;i<new_items.length;i++) {
                            let mi = plindex.model.searchRecursive(
                                new_items[i],
                                "actorUuidRole", plindex.model.index(0,0,plindex)
                            )
                            if(mi.valid)
                                indexes.push(mi)
                        }

                        if(indexes.length) {
                            mediaSelectionModel.select(
                                helpers.createItemSelection(indexes),
                                mode | ItemSelectionModel.setCurrentIndex
                            )
                        }

                        if(playhead_index != -1)
                            current_playhead.keySubplayheadIndex = playhead_index
                    }
                } ( index, quuids ), 1000);
            }
        }

        // This function is called when the selectionRole data of the current
        // PlaylistSelection actor changes. The selectionRole data is a list
        // of uuids of media items that are selected. It should match the
        // selection of THIS (the mediaSelectionModel) but if the selection has
        // been changed in the backend only then we need to update ourselves.
        function updateSelection(uuids) {

            // first get uuids of our frontend selection
            var selected_uuids = []
            for (var i in selectedIndexes) {
                selected_uuids.push(
                    sessionData.get(
                        selectedIndexes[i],
                        "actorUuidRole")
                        )
            }

            // check if it already matches the incoming backend selection
            var needSync = false
            if (selected_uuids.length == uuids.length) {
                for (var i in selected_uuids) {
                    if (selected_uuids[i] != uuids[i]) {
                        needSync = true
                        break
                    }
                }
            } else {
                needSync = true
            }

            if (!needSync) return

            // make sure we don't trigger backend updates as we update
            // the mediaSelectionModel
            updateBackend = false
            let new_selection = []
            for (var i in uuids) {
                let idx = sessionData.searchRecursive(
                    uuids[i],
                    "actorUuidRole", currentMediaContainerIndex,
                    0,
                    2 // depth of 2 - stops the search hitting a match in a subset if we are searching a playlist
                    )

                if (idx.valid) {
                    new_selection.push(idx)
                }
            }

            mediaSelectionModel.select(
                helpers.createItemSelection(new_selection),
                ItemSelectionModel.ClearAndSelect
            )

            updateBackend = true
        }

        function getSourceIndex(mediaIndex) {
            let m = mediaIndex.model
            let ms = m.searchRecursive(m.get(mediaIndex, "imageActorUuidRole"), "actorUuidRole", mediaIndex)
            return ms
        }

        function getSelectedMediaUrl(role="pathRole") {
            let result = []
            let sel = mediaSelectionModel.selectedIndexes
            for(let i =0;i<sel.length;i++) {
                let mi = sel[i]
                let ms = mi.model.searchRecursive(mi.model.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)

                if(ms.valid) {
                    let v = mi.model.get(ms, role)
                    if(v != undefined)
                        result.push(v)
                }
            }
            return result;
        }
    }

    // mediaListModelData will give us the filtered media list that is used
    // to drive the "media list" widgets
    property string mediaListSearchString
    XsMediaListModelData {
        id: filteredMediaListData
    }
    property alias mediaListModelDataRoot: filteredMediaListData.rootIndex
    property alias mediaListModelData: filteredMediaListData.model
    onMediaListSearchStringChanged: {
        session.updateMediaListFilterString(playheadSelectionIndex, mediaListSearchString)
    }

    property alias mediaSelectionModel: mediaSelectionModel

    /* This model gives us access to the playlists */
    DelegateModel {

        id: playlistsModelData

        // this is required as "model" doesn't issue notifications on change
        property var notifyModel: sessionData

        // our model is the main sessionData instance
        model: notifyModel

        // the playlists in the session start one level down at row=0, column=0.
        rootIndex: sessionData.playlistsRootIdx

    }
    property alias playlistsModelData: playlistsModelData

    /* This gives us direct access to the properties of the inspected
    playlist - in other words the playlist that is showing in the media list */
    XsModelPropertyMap {
        id: inspectedMediaSetProperties
        index: currentMediaContainerIndex
    }
    property alias inspectedMediaSetProperties: inspectedMediaSetProperties

    /* This gives us direct access to the properties of the current (active)
    playlist - for example viewedMediaSetProperties.values.nameRole gives us
    the playlist name */
    XsModelPropertyMap {
        id: viewedMediaSetProperties
        index: sessionData.viewportCurrentMediaContainerIndex
    }
    property alias viewedMediaSetProperties: viewedMediaSetProperties

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


    /* This model is a simple list of the panels that xSTUDIO can provide.
    The data of each row is the name of panel and the QML code needed to
    instantiate the widget that is the panel. xSTUDIO plugins can add data
    to the views model and this is how plugins can insert their own panels into
    the xstudio interface */
    XsViewsModel {
        id: views_model
    }
    property alias viewsModel: views_model


    /* This model is a simple list of the pop-out windows that are accessible
    via buttons in the shelf at the top-left of the viewport panel. e.g. notes
    or grading tool. */
    XsPopoutWindowsData {
        id: popout_windows
    }
    property alias popoutWindowsModel: popout_windows


    /* This model is a flat list with just one attribute 'source' which is qml
    that we want to load once only. This allows plugins to declare some qml
    that is instanced once only at the global level. It is a good way to add
    menu items at the application level */
    XsSingletonItemsModel {
        id: singletons_model
    }
    property alias singletonsModel: singletons_model

    /* This model gives access to bookmark/notes data */
    XsBookmarkModel {
        id: bookmarkModel
        bookmarkActorAddr: sessionData.bookmarkActorAddr
    }
    property alias bookmarkModel: bookmarkModel


}