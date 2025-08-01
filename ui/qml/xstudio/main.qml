// SPDX-License-Identifier: Apache-2.0
// import QtQuick.Controls 1.4

import Qt.labs.platform 1.1
import Qt.labs.settings 1.0
import QtGraphicalEffects 1.12
import QtQml 2.15
import QtQml.Models 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.12
import QtQuick.Window 2.15

import QuickFuture 1.0
import QuickPromise 1.0

//------------------------------------------------------------------------------
// BEGIN COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------
import xstudio.qml.bookmarks 1.0
import xstudio.qml.clipboard 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.event 1.0
import xstudio.qml.global_store_model 1.0
import xstudio.qml.module 1.0
// import xstudio.qml.playlist 1.0
import xstudio.qml.semver 1.0
import xstudio.qml.session 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.bookmarks 1.0

//------------------------------------------------------------------------------
// END COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------

import xStudio 1.0


ApplicationWindow {
    id: app_window
    visible: true
    color: XsStyle.theme == "dark"?"#00000000":"white"
    title: (sessionPathNative ? (app_window.sessionModel.modified ? sessionPathNative + " - modified": sessionPathNative) : "xStudio")
    objectName: "appWindow"
    minimumWidth: sessionWidget.minimumWidth
    minimumHeight: sessionWidget.minimumHeight
    property color highlightColor: XsStyle.highlightColor
    property bool initialized: false

    // override default palette
    palette.base: XsStyle.controlBackground
    palette.text: XsStyle.hoverColor
    palette.button: XsStyle.controlTitleColor
    palette.highlight: highlightColor
    palette.light: highlightColor
    palette.highlightedText: XsStyle.mainBackground
    palette.brightText: highlightColor
    palette.buttonText: XsStyle.hoverColor
    palette.windowText: XsStyle.hoverColor

    // so visible clients can action requests.
    signal flagSelectedItems(string flag)

    // palette.alternateBase: "Red"
    // palette.dark: "Red"
    // palette.link: "Red"
    // palette.linkVisited: "Red"
    // palette.mid: "Red"
    // palette.midlight: "Blue"
    // palette.shadow: "Red"
    // palette.toolTipBase: "Red"
    // palette.toolTipText: "Red"
    // palette.window: "Red"

    property var preFullScreenVis: [app_window.x, app_window.y, app_window.width, app_window.height]
    property var qmlWindowRef: Window  // so javascript can reference Window enums.

    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Regular.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Black.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-BlackItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Bold.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-BoldItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-ExtraBold.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-ExtraBoldItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-ExtraLight.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-ExtraLightItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Italic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Light.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-LightItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-SemiBold.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-SemiBoldItalic.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-Thin.ttf"}
    FontLoader {source: "qrc:/fonts/Overpass/Overpass-ThinItalic.ttf"}
    FontLoader {source: "qrc:/fonts/BitstreamVeraMono/VeraMono.ttf"}

    // // remove focus from widgets when clicking outside of them
    // Pane {
    //     anchors.fill: parent
    //     focusPolicy: Qt.ClickFocus
    // }
    property bool dimmer: false

    Popup {
        modal: true
        visible: dimmer
        width: 0
        height: 0
    }

    Clipboard {
      id: clipboard
    }

    XsGlobalStoreModel {
        id: globalStoreModel
    }
    property alias globalStoreModel: globalStoreModel


    XsEvent {
        id: events
    }
    property var events: events

    XsWindowStateSaver
    {
        windowObj: app_window
        windowName: "main_window"
    }

    // Apply the shown state of the second window from last session

    XsModelNestedPropertyMap {
        id: second_window_settings
        index: app_window.globalStoreModel.search_recursive("/ui/qml/second_window_settings", "pathRole")
        property alias properties: second_window_settings.values
    }

    property alias bookmark_categories: bookmark_categories

    XsModelProperty {
        id: bookmark_categories_value
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/bookmark/category", "pathRole")
    }

    property alias mediaFlags: mediaFlags

    XsModelPropertyTree {
        id: mediaFlags
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/media_flags", "pathRole")
    }

    XsBookmarkCategories {
        id: bookmark_categories
        value: bookmark_categories_value.value
    }

    Timer {
        id: feature_timer
        repeat: false
        interval: 500
        onTriggered: semantic_version.show_features()
    }

    Timer {
        id: restore_auto_save_timer
        repeat: false
        interval: 2000
        onTriggered: check_restore_autosave()
    }

    Component.onCompleted: {
        if (second_window_settings.properties.visibility != 0) togglePopoutViewer()

        feature_timer.start()
        restore_auto_save_timer.start()
    }

    XsGlobalPreferences {
        id: preferences

        Component.onCompleted: {
           styleGradient.highlightColorString = highlightColor+""
        }
    }
    property alias preferences: preferences

    XsStyleGradient {
        id: styleGradient
    }
    property alias styleGradient: styleGradient

    SemVer {
        id: semantic_version
        version: Qt.application.version

           function show_features() {

            if (semantic_version.compare(preferences.latest_version.value) > 0) {
                XsUtils.openDialog("qrc:/dialogs/XsFunctionalFeaturesDialog.qml").open()
            }
            semantic_version.store()
        }

        function store() {
            if(semantic_version.compare(preferences.latest_version.value) > 0) {
                // immediate update..
                preferences.latest_version.value = Qt.application.version
            }
        }
    }

    function check_restore_autosave() {
        if(preferences.last_auto_save.value !== "") {
            restoreSession.autosave_path = preferences.last_auto_save.value
            restoreSession.open()
        }
    }

    function toggleLogDialog() {
        log_dialog.toggle()
    }

    Connections {
        target: logger
        function onNewLogRecord(timestamp, level, text) {
            // show on critical
            if(level >= log_dialog.showLevel)
                log_dialog.show()
        }
    }

    XsLogDialog {
        id: log_dialog
    }

    XsButtonDialog {
        property var autosave_path: ""
        id: restoreSession
        // parent: sessionWidget
        width: 500
        title: "Restore Autosaved Session"
        text: {
            let tmp = autosave_path.split("/")
            return tmp[tmp.length-1]
        }
        buttonModel: ["Cancel", "Restore"]
        onSelected: {
            if(button_index == 1) {
                Future.promise(studio.loadSessionFuture(encodeURIComponent(autosave_path))).then(function(result){})
            }
        }
    }

    function launchQuickViewerWithSize(sources, compare_mode, __position, __size) {

        var component = Qt.createComponent("player/XsLightPlayerWindow.qml");
        if (component.status == Component.Ready) {
            if (compare_mode == "Off" || compare_mode == "") {
                for (var source in sources) {
                    var light_viewer = component.createObject(app_window, {x: __position.x, y: __position.y, width: __size.width, height: __size.height, sessionModel: sessionModel});
                    light_viewer.show()
                    light_viewer.viewport.quickViewSource([sources[source]], "Off")
                    light_viewer.raise()
                    light_viewer.requestActivate()
                    light_viewer.raise()
                }
            } else {
                var light_viewer = component.createObject(app_window, {x: __position.x, y: __position.y, width: __size.width, height: __size.height, sessionModel: sessionModel});
                light_viewer.show()
                light_viewer.viewport.quickViewSource(sources, compare_mode)
                light_viewer.raise()
                light_viewer.requestActivate()
                light_viewer.raise()
        }
        } else {
            // Error Handling
            console.log("Error loading component:", component.errorString());
        }
    }

    // QuickView window position management
    property var quickWinPosition: Qt.point(100, 100)
    property var quickWinSize: Qt.size(1280,720)
    property bool quickWinPosSet: false

    function closingQuickviewWindow(position, size) {
        // when a QuickView window is closed, remember its size and position and
        // re-use for next QuickView window
        quickWinPosition = position
        quickWinSize = size
        quickWinPosSet = true
    }

    function launchQuickViewer(sources, compare_mode) {
        launchQuickViewerWithSize(sources, compare_mode, quickWinPosition, quickWinSize)
        if (quickWinPosSet) {
            // rest the default position for the next QuickView window
            quickWinPosition = Qt.point(100, 100)
            quickWinPosSet = false
        } else {
            // each new window will be positioned 100 pixels to the bottom and
            // right of the previous one
            quickWinPosition = Qt.point(quickWinPosition.x + 100, quickWinPosition.y + 100)
        }
    }

    width: 1280
    height: 820

    function togglePopoutViewer() {
        popout_window.toggle_visible()
    }

    function toggleFullscreen() {
        if (visibility !== Window.FullScreen) {
            preFullScreenVis = [x, y, width, height]
            showFullScreen();
        } else {
            visibility = qmlWindowRef.Windowed
            x = preFullScreenVis[0]
            y = preFullScreenVis[1]
            width = preFullScreenVis[2]
            height = preFullScreenVis[3]
        }
    }

    function normalScreen() {
        if (visibility == Window.FullScreen) {
            toggleFullscreen()
        }
    }

    XsButtonDialog {
        id: overwriteDialog
        // parent: sessionWidget
        title: "Session File Modified"
        text: "Session file externally modified, overwrite?"
        width: 500
        buttonModel: ["Cancel", "Overwrite", "Save As..."]

        onSelected: {
            if(button_index == 1) {
                sessionFunction.saveSessionPath(sessionPath).then(function(result){
                    if (result != "") {
                        var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                        dialog.title = "Save session failed"
                        dialog.text = result
                        dialog.show()
                    } else {
                        sessionFunction.newRecentPath(sessionPath)
                    }
                })
            } else if(button_index == 2) {
                sessionFunction.saveSessionAs()
            }
        }
    }

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupNames: "playhead"
    }

    XsBookmarkModel {
        id: bookmarkModel
        bookmarkActorAddr: sessionModel.bookmarkActorAddr
    }

    property alias playlistModel: sessionModel.playlists

    property alias sessionModel: sessionModel

    XsSessionModel {
        id: sessionModel
        sessionActorAddr: studio.sessionActorAddr

        onPlaylistsChanged: {
            if(sessionSelectionModel.currentIndex == undefined || !sessionSelectionModel.currentIndex.valid) {
                let ind = sessionModel.search_recursive("Playlist", "typeRole")
                if(ind.valid) {
                    currentSource.index = ind
                    screenSource.index = ind
                    sessionSelectionModel.select(ind, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
                    sessionSelectionModel.setCurrentIndex(ind, ItemSelectionModel.setCurrentIndex)
                }
            }
        }

        onSessionActorAddrChanged: {
            currentSource.index = sessionModel.index(-1,-1)
            screenSource.index = sessionModel.index(-1,-1)
            mediaImageSource.index = sessionModel.index(-1,-1)
            sessionSelectionModel.clear()
        }
        onModelReset: {
            sessionPropertyMap.index = sessionModel.index(0,0)
        }
    }

    property alias sessionSelectionModel: sessionSelectionModel
    ItemSelectionModel {
        id: sessionSelectionModel
        model: sessionModel

        onCurrentChanged: {
            currentSource.index = currentIndex
            sessionModel.setCurrentPlaylist(currentSource.index)
        }
    }


    property alias sessionExpandedModel: sessionExpandedModel
    ItemSelectionModel {
        id: sessionExpandedModel
        model: sessionModel
    }

    property var sessionPath: sessionPropertyMap.values.pathRole
    property var sessionRate: sessionPropertyMap.values.rateFPSRole
    property var sessionMTime: sessionPropertyMap.values.mtimeRole
    property var sessionPathNative: sessionPath ? helpers.pathFromURL(sessionPath) : ""

    XsModelPropertyMap {
        id: sessionPropertyMap
    }

    XsTimer {
        id: delayTimer
    }

    property alias currentSource: currentSource
    XsModelPropertyMap {
        id: currentSource
        index: sessionModel.index(-1,-1)
        property var fullName: values.nameRole ? getFullName() : getFullName()

        onIndexChanged: {
            // console.log("onIndexChanged", currentSource.index)
            if(currentSource.index.valid) {
                if(!screenSource.index.valid)
                    screenSource.index = currentSource.index
                mediaSelectionModel.updateSelectionFromBackend(currentSource.index.model.index(1, 0, currentSource.index))
                // always something selected..

                if(!mediaSelectionModel.selectedIndexes.length) {
                    // get first item..
                    let mcind = sessionModel.index(0, 0, currentSource.index)
                    if(mcind.valid) {
                        if(sessionModel.rowCount(mcind)) {
                            mediaSelectionModel.select(sessionModel.index(0,0,mcind), ItemSelectionModel.ClearAndSelect)
                        } else {
                            delayTimer.setTimeout(function() {
                                if(sessionModel.rowCount(mcind)) {
                                    mediaSelectionModel.select(sessionModel.index(0,0,mcind), ItemSelectionModel.ClearAndSelect)
                                }
                            }, 200)
                        }
                    }
                }
            } else {
                screenSource.index = currentSource.index
                mediaSelectionModel.clear();
            }
        }

        function getFullName() {
            let result = ""
            if(index.valid) {
                if(values.typeRole == "Playlist") {
                    result = values.nameRole ? values.nameRole : ""
                }
                else {
                    result = index.model.get(index.parent.parent, "nameRole") + " - " + values.nameRole
                }
            }
            return result
        }
    }

    // current MT_IMAGE media source
    property alias mediaImageSource: mediaImageSource
    XsModelPropertyMap {
        id: mediaImageSource
        index: sessionModel.index(-1,-1)

        property var fileName: {
            let result = ""
            if(index.valid && values.pathRole != undefined) {
                result = helpers.fileFromURL(values.pathRole)
            }
            return result
        }

        property var streams: []

        function buildStreams() {
            let result = []
            if(index.valid) {
                let ind = index.model.index(0, 0, index)
                if(ind.valid) {
                    for(let i=0; i<index.model.rowCount(ind); i++) {
                        let sind = ind.model.index(i, 0, ind)
                        result.push({"name": index.model.get(sind, "nameRole"), "uuid": index.model.get(sind, "actorUuidRole")})
                    }
                }
            }
            streams = result
        }

        onIndexChanged: {
            // console.log("*****************************mediaImageSource, onIndexChanged", index)
            screenMedia.index = index.parent
            if(index.valid) {
                // we need these populated first..
                if(index.model.rowCount(index)) {
                    // stream containers loaded.
                    let ia = index.model.search_recursive(values.imageActorUuidRole, "actorUuidRole", index.model.index(0,0,index))
                    let aa = index.model.search_recursive(values.audioActorUuidRole, "actorUuidRole", index.model.index(1,0,index))

                    // console.log("imageStreams", index.model.index(0,0,index), index.model.rowCount(index.model.index(0,0,index)))
                    // console.log("audioStreams", index.model.index(1,0,index), index.model.rowCount(index.model.index(1,0,index)))
                    // console.log("imageActorUuidRole", ia, index.model.get(ia, "nameRole"))
                    // console.log("audioActorUuidRole", aa, index.model.get(aa, "nameRole"))
                    buildStreams()
                } else {
                    callback_timer.setTimeout(function(plindex) { return function() {
                        // console.log(index.model.rowCount(index))
                        if(index.model) {
                            let ia = index.model.search_recursive(values.imageActorUuidRole, "actorUuidRole", index.model.index(0,0,index))
                            let aa = index.model.search_recursive(values.audioActorUuidRole, "actorUuidRole", index.model.index(1,0,index))
                            buildStreams()
                        }
                    }}( index ), 100);
                }
            } else {
                buildStreams()
            }
        }
    }

    // update current media under playhead.
    // media can exist in multiple parts of the tree..
    // make sure we're looking in the right one..
    function updateMediaUuid(uuid) {
        let media_idx = app_window.sessionModel.search(uuid, "actorUuidRole", app_window.sessionModel.index(0,0,screenSource.index), 0)
        if(media_idx.valid) {
            // get index of active source.
            let active = media_idx.model.get(media_idx, "imageActorUuidRole")

            // dummy to populate..
            media_idx.model.get(media_idx, "audioActorUuidRole")

            if(active != undefined) {
                let msi = app_window.sessionModel.search(active, "actorUuidRole", media_idx)

                if(mediaImageSource.index != msi)
                    mediaImageSource.index = msi

                let msip = mediaImageSource.index.parent
                if(mediaSelectionModel.currentIndex != msip) {
                    mediaSelectionModel.setCurrentIndex(msip, ItemSelectionModel.setCurrentIndex)
                }
            }
            else
                mediaImageSource.index = app_window.sessionModel.index(-1,-1)
        } else {
            mediaImageSource.index = app_window.sessionModel.index(-1,-1)
        }
    }

    Connections {
        target: viewport.playhead
        function onMediaUuidChanged(uuid) {
            updateMediaUuid(uuid)
        }
    }


    property alias screenSource: screenSource
    XsModelPropertyMap {
        id: screenSource
        index: sessionModel.index(-1,-1)
        onIndexChanged: {
            sessionModel.setPlayheadTo(index)
        }
    }

    property alias screenMedia: screenMedia
    XsModelPropertyMap {
        id: screenMedia
        index: mediaImageSource.index
    }

    XsTimer {
        id: m_timer
    }

    // manages media selection, for current source.
    property alias mediaSelectionModel: mediaSelectionModel
    ItemSelectionModel {
        id: mediaSelectionModel
        model: sessionModel

        // current follows current playhead media ?
        onCurrentChanged: {
            if(current.valid) {
                // check playhead...
                let active = current.model.get(current, "actorUuidRole")
                // console.log(active, viewport.playhead.mediaUuid)
                if(active != viewport.playhead.mediaUuid) {
                    viewport.playhead.jumpToSource(active)
                }
                // find media source index..
                let mind = current.model.search_recursive(current.model.get(current, "imageActorUuidRole"), "actorUuidRole", current)
                if(mind.valid) {
                    if(mediaImageSource.index != mind)
                        mediaImageSource.index = mind
                } else {
                    // children not valid wait a little..
                    m_timer.setTimeout(function(index) { return function() {
                        let model = index.model
                        let mind = model.search_recursive(model.get(index, "imageActorUuidRole"), "actorUuidRole", index)
                        if(mind.valid && mediaImageSource.index != mind) {
                            mediaImageSource.index = mind
                        }

                    }}( current ), 100);

                }
            }
        }

        onSelectionChanged: {
            // console.log("onSelectionChanged", selectedIndexes)
            if(currentSource.index.valid && selectedIndexes.length) {
                model.updateSelection(currentSource.index.model.index(1, 0, currentSource.index), selectedIndexes)
            }
        }

        function updateSelectionFromBackend(parent) {
            // console.log("updateSelectionFromBackend", parent)
            if(currentSource.index.valid) {
                let cmodel = currentSource.index.model
                let pmodel = parent.model
                let ind = cmodel.index(1, 0, currentSource.index)
                if(pmodel && ind == parent) {
                    // get list and update.
                    let count = cmodel.rowCount(ind)
                    let indexes = []
                    for(let i =0; i<count;i++) {
                        let m_uuid = pmodel.get(pmodel.index(i, 0, parent), "uuidRole")
                        // actorUUid of media..
                        indexes.push(
                            parent.model.search_recursive(
                                pmodel.get(pmodel.index(i, 0, parent), "uuidRole"),
                                "actorUuidRole",
                                cmodel.index(0, 0, currentSource.index)
                            )
                        )
                    }

                    mediaSelectionModel.select(helpers.createItemSelection(indexes), ItemSelectionModel.ClearAndSelect)
                }
            }
        }
    }

    // need to capture changes to PlayheadSelection model.
    Connections {
        target: sessionModel
        function onRowsRemoved(parent, first, last) {
            mediaSelectionModel.updateSelectionFromBackend(parent)
            // check still valid..
            if(app_window.mediaImageSource.index && !app_window.mediaImageSource.index.valid) {
                app_window.mediaImageSource.index = sessionModel.index(-1,-1)
            }
        }
        function onRowsInserted(parent, first, last) {
            mediaSelectionModel.updateSelectionFromBackend(parent)
        }
        function onRowsMoved(parent, first, count, target, first) {
            mediaSelectionModel.updateSelectionFromBackend(parent)
        }

        function onMediaSourceChanged(media_index, media_source_index, media_type) {
            if(media_type == 1 && mediaSelectionModel.currentIndex == media_index && mediaImageSource.index != media_source_index) {
                mediaImageSource.index = media_source_index
            }
        }
    }

    property alias sessionFunction: sessionFunction
    property alias bookmarkModel: bookmarkModel

    Item {
        id: sessionFunction
        property var checked_unsaved_session: false

        property var object_map: ({})

        function setScreenSource(index) {
            screenSource.index = index
        }

        XsTimer {
          id: callback_timer
        }

        XsStringRequestDialog {
            id: request_new
            // y: centerOn ? centerOn.mapToGlobal(0, 25).y : 0
            keepCentered: false
            okay_text: ""
            text: ""

            property var index: null
            property string type: ""

            XsTimer {
                id: timelineReady
            }

            function addTracks(timeline_index) {
                delayTimer.setTimeout(function() {
                    let model = timeline_index.model;

                    let timelineItemIndex = model.index(2,0,timeline_index)
                    if(timelineItemIndex.valid) {
                        let stackIndex = model.index(0,0,timelineItemIndex)
                        if(stackIndex.valid) {
                            app_window.sessionModel.insertRowsSync(0, 1, "Audio Track", "Audio Track", stackIndex)
                            app_window.sessionModel.insertRowsSync(0, 1, "Video Track", "Video Track", stackIndex)
                        } else {
                            addTracks(timeline_index)
                        }
                    } else {
                        addTracks(timeline_index)
                    }
                }, 100)

            }

            onOkayed: {
                let new_indexes = index.model.insertRowsSync(
                    index.model.rowCount(index),
                    1,
                    type, text,
                    index
                )
                if(type == "Timeline") {
                    index.model.index(2, 0, new_indexes[0])
                    addTracks(new_indexes[0])
                }

                app_window.sessionExpandedModel.select(index.parent, ItemSelectionModel.Select)
            }
        }

        XsButtonDialog {
            id: remove_selected
            text: "Remove Selected"
            width: 300
            buttonModel: ["Cancel", "Remove"]
            onSelected: {
                if(button_index == 1) {
                    // order by reverse row..
                    let items = []
                    sessionSelectionModel.selectedIndexes.forEach(function (item, index) {
                        items.push(item)
                    })
                    items = items.sort((a,b) => b.row - a.row )
                    items.forEach(function (item, index) {
                        item.model.removeRows(item.row, 1, false, item.parent)
                    })
                }
            }
        }

        XsStringRequestDialog {
            id: request_media_rate
            input.inputMethodHints: Qt.ImhFormattedNumbersOnly
            okay_text: "Set Media Rate"
            onOkayed: sessionFunction.setMediaRate(text)
            // y: centerOn ? centerOn.mapToGlobal(0, 25).y : 0
            // centerOn: null
        }

        XsStringRequestDialog {
            id: add_media_to_playlist
            okay_text: "Copy Media"
            secondary_okay_text: "Move Media"
            text: "Untitled Playlist"

            onOkayed: {
                let selection = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                let index = app_window.sessionFunction.createPlaylist(text)
                callback_timer.setTimeout(function(plindex, selection) { return function() {
                    plindex.model.copyRows(selection, 0, plindex)
                }}( index, selection ), 100);

            }

            onSecondary_okayed: {
                let selection = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                let index = app_window.sessionFunction.createPlaylist(text)
                callback_timer.setTimeout(function(plindex, selection) { return function() {
                    plindex.model.moveRows(selection, 0, plindex)
                }}( index, selection ), 100);
            }

            // y: playlist_panel.mapToGlobal(0, 25).y
            // centerOn: playlist_panel

        }

        XsStringRequestDialog {
            id: add_media_to_subset
            okay_text: "Add Media"
            text: "Untitled Subset"

            onOkayed: {
                // find current playlist
                let ind = app_window.sessionFunction.firstSelected("Playlist")

                if(ind != null && ind.valid)  {
                    let media = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                    let subset = sessionModel.insertRowsSync(
                        sessionModel.rowCount(sessionModel.index(2,0, ind)),
                        1,
                        "Subset", text,
                        sessionModel.index(2,0, ind)
                    )[0]

                    callback_timer.setTimeout(function(plindex, mediaind) { return function() {
                        plindex.model.copyRows(mediaind, 0, plindex)
                    }}( subset, media ), 100);
                }
            }
        }

        XsStringRequestDialog {
            id: add_media_to_timeline
            okay_text: "Add Media"
            text: "Untitled Timeline"

            onOkayed: {
                // find current playlist
                let ind = app_window.sessionFunction.firstSelected("Playlist")

                if(ind != null && ind.valid)  {
                    let media = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                    let subset = sessionModel.insertRowsSync(
                        sessionModel.rowCount(sessionModel.index(2,0, ind)),
                        1,
                        "Timeline", text,
                        sessionModel.index(2,0, ind)
                    )[0]

                    callback_timer.setTimeout(function(plindex, mediaind) { return function() {
                        plindex.model.copyRows(mediaind, 0, plindex)
                    }}( subset, media ), 100);
                }
            }
        }

        XsButtonDialog {
            id: remove_media
            // parent: sessionWidget.media_list
            text: "Remove Selected Media"
            width: 300
            buttonModel: ["Cancel", "Remove"]
            onSelected: {
                if(button_index == 1) {
                    sessionFunction.removeSelectedMedia()
                }
            }
        }

        FileDialog {
            id: media_dialog
            title: "Select media files"
            folder: app_window.sessionFunction.defaultMediaFolder() || shortcuts.home

            nameFilters:  [ "Media files ("+helpers.validMediaExtensions()+")", "All files (*)" ]
            selectExisting: true
            selectMultiple: true

            property var index: null

            onAccepted: {
                let uris = ""
                media_dialog.fileUrls.forEach(function (item, index) {
                    uris = uris + String(item) +"\n"
                })

                if(index == null) {
                    // create new playlist..
                    index = app_window.sessionFunction.createPlaylist("Add Media")
                    callback_timer.setTimeout(function(capture) { return function(){
                        Future.promise(
                            capture.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": uris}, capture)
                        ).then(function(quuids){
                            app_window.sessionFunction.selectNewMedia(index,quuids)
                        }) }}(index), 100
                    );
                }
                else
                    Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": uris}, index)).then(
                        function(quuids){
                            app_window.sessionFunction.selectNewMedia(index,quuids)
                        }
                    )

                app_window.sessionFunction.defaultMediaFolder(folder)
            }
        }

        FileDialog {
            id: relink_media_dialog
            title: "Relink media files"
            folder: app_window.sessionFunction.defaultMediaFolder() || shortcuts.home

            selectFolder: true
            selectExisting: true
            selectMultiple: false

            onAccepted: {
                app_window.sessionModel.relinkMedia(mediaSelectionModel.selectedIndexes, relink_media_dialog.fileUrls[0])
                app_window.sessionFunction.defaultMediaFolder(folder)
            }
        }

        FileDialog {
            id: import_sequence_dialog
            title: "Select sequence files"
            folder: app_window.sessionFunction.defaultMediaFolder() || shortcuts.home

            nameFilters:  [ "All files (*)" ]
            selectExisting: true
            selectMultiple: true

            property var index: null

            onAccepted: {
                let uris = ""
                import_sequence_dialog.fileUrls.forEach(function (item, index) {
                    uris = uris + String(item) +"\n"
                })

                if(index == null) {
                    // create new playlist..
                    index = app_window.sessionFunction.createPlaylist("Imported Sequence")
                    callback_timer.setTimeout(function(capture) { return function(){
                    Future.promise(
                        capture.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": uris}, capture)
                    ).then(function(quuids){
                        // console.log("handleDropFuture finished")
                    }) }}(index), 100);
                }
                else
                    Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": uris}, index)).then(function(quuids){})

                app_window.sessionFunction.defaultMediaFolder(folder)
            }
        }

        function selectNewMedia(index, quuids) {
            let type = index.model.get(index,"typeRole")
            // console.log(index, quuids)

            if(quuids.length && ["Playlist", "Subset", "Timeline"].includes(type)) {
                app_window.sessionFunction.setActivePlaylist(index)
                callback_timer.setTimeout(function(plindex, new_item) { return function() {
                    let new_media = plindex.model.search_recursive(new_item, "actorUuidRole", plindex.model.index(0,0,plindex))
                    if(new_media.valid) {
                        app_window.sessionFunction.setActiveMedia(new_media)
                    }
                }}( index, "{"+helpers.QUuidToQString(quuids[0])+"}" ), 1000);
            }
        }

        function createPlaylist(name, sync=true) {
            if(sync)
                return sessionModel.insertRowsSync(
                    sessionModel.rowCount(sessionModel.index(0,0)),
                    1,
                    "Playlist", name,
                    sessionModel.index(0,0)
                )[0]

            return sessionModel.insertRowsAsync(
                sessionModel.rowCount(sessionModel.index(0,0)),
                1,
                "Playlist", name,
                sessionModel.index(0,0)
            )[0]
        }

        function defaultMediaFolder(path=null) {
            if(path != null) {
                preferences.default_media_folder.value = path
            } else {
                if(preferences.default_media_folder.value != "") {
                    return preferences.default_media_folder.value
                }
            }
            return null
        }

        function defaultSessionFolder() {
            // pick more recent path..
            if(preferences.recent_history.value.length) {
                let p = preferences.recent_history.value[0]
                return p.substr(0, p.lastIndexOf("/"))
            }

            return null
        }

        function jumpToNextSource() {
            if (!viewport.playhead.jumpToNextSource()) {
                // get selection..
                let selind = currentSource.index.model.index(1, 0, currentSource.index)
                let medind = currentSource.index.model.index(0, 0, currentSource.index)
                let mcount = medind.model.rowCount(medind)
                let last_uuid  = medind.model.get(medind.model.index(mcount-1,0,medind), "actorUuidRole")
                let first_suuid  = selind.model.get(selind.model.index(0,0,selind), "uuidRole")

                if(preferences.cycle_through_playlist.value && last_uuid == first_suuid){
                    currentSource.index.model.moveSelectionByIndex(selind, -(mcount - 1))
                } else {
                    currentSource.index.model.moveSelectionByIndex(selind, 1)
                }
            }
        }

        function jumpToPreviousSource() {
            if (!viewport.playhead.jumpToPreviousSource()) {

                let selind = currentSource.index.model.index(1, 0, currentSource.index)
                let medind = currentSource.index.model.index(0, 0, currentSource.index)
                let mcount = medind.model.rowCount(medind)
                let first_uuid  = medind.model.get(medind.model.index(0,0,medind), "actorUuidRole")
                let first_suuid  = selind.model.get(selind.model.index(0,0,selind), "uuidRole")

                if(preferences.cycle_through_playlist.value && first_uuid == first_suuid){
                    currentSource.index.model.moveSelectionByIndex(selind, mcount - 1)
                } else {
                    currentSource.index.model.moveSelectionByIndex(selind, -1)
                }
            }
        }

        function setActivePlaylist(index) {
            app_window.sessionSelectionModel.select(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
            app_window.sessionSelectionModel.setCurrentIndex(index, ItemSelectionModel.setCurrentIndex)
            app_window.screenSource.index = index
        }

        function setActiveMedia(index, clear=true) {
            app_window.mediaSelectionModel.setCurrentIndex(index, ItemSelectionModel.setCurrentIndex)
            if(clear)
                app_window.mediaSelectionModel.select(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
            else
                app_window.mediaSelectionModel.select(index, ItemSelectionModel.Select | ItemSelectionModel.setCurrentIndex)
        }


        function addMedia(index) {
            media_dialog.index = index
            media_dialog.open()
        }

        function addMediaFromClipboard(index) {
            if(clipboard.text.length) {
                let ct = ""
                clipboard.text.split("\n").forEach(function (item, index) {
                        // replace #'s
                        // item.replace(/[#]+/, "*")
                        ct = ct + "file://" + item + "\n"
                    }
                )
                if(index == null) {
                    // create new playlist..
                    index = createPlaylist("Clipboard")
                    // console.log(index, ct)
                    Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": ct}, index)).then(function(quuids){
                        app_window.sessionFunction.selectNewMedia(index, quuids)
                    })
                }
                else {
                    // console.log(index, ct)
                    Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": ct}, index)).then(function(quuids){
                        app_window.sessionFunction.selectNewMedia(index, quuids)
                    })
                }
            }
        }

        function removeSelected() {
            remove_selected.open()
        }

        function newPlaylist(index, text=null, centeron=null) {
            if(index != null) {
                request_new.text = "Untitled Playlist"
                request_new.okay_text = "Add Playlist"
                request_new.type = "Playlist"
                // request_new.centerOn = centeron
                request_new.index = index

                if(text != null)
                    request_new.text = text

                request_new.open()
            }
        }

        function newDivider(index, text=null, centeron=null) {
            if(index != null) {
                request_new.text = "Untitled Divider"
                request_new.okay_text = "Add Divider"
                request_new.type = "ContainerDivider"
                request_new.centerOn = centeron
                request_new.index = index

                if(text != null)
                    request_new.text = text

                request_new.open()
            }
        }


        function newSubset(index, text=null, centeron=null) {
            if(index != null) {
                request_new.text = "Untitled Subset"
                request_new.okay_text = "Add Subset"
                request_new.type = "Subset"
                request_new.centerOn = centeron
                request_new.index = index

                if(text != null)
                    request_new.text = text

                request_new.open()
            }
        }

        function newTimeline(index, text=null, centeron=null) {
            if(index != null) {
                request_new.text = "Untitled Timeline"
                request_new.okay_text = "Add Timeline"
                request_new.type = "Timeline"
                request_new.centerOn = centeron
                request_new.index = index

                if(text != null)
                    request_new.text = text

                request_new.open()
            }
        }

        function getPlaylistIndex(index) {
            if(index && index.valid) {
                let model = index.model
                let type = model.get(index, "typeRole")
                if(type == "Playlist")
                    return index
                else if( type == "Subset" )
                    return index.parent.parent
                else if( type == "Timeline" )
                    return index.parent.parent
            }
            return null
        }

        function firstSelected(type) {
            let model = sessionSelectionModel.model
            if(sessionSelectionModel.currentIndex) {
                return getPlaylistIndex(sessionSelectionModel.currentIndex)
            } else {
                for(let i =0; i< sessionSelectionModel.selectedIndexes.length; i++) {
                    let ind = sessionSelectionModel.selectedIndexes[i]
                    if(ind.model.get(ind,"typeRole") == type)
                        return ind
                }
            }

            return null
        }

        function mergeSelected() {
            if(sessionSelectionModel.selectedIndexes.length) {
                sessionModel.mergeRows(sessionSelectionModel.selectedIndexes)
            }
        }

        function flagSelected(flag, flag_text="") {
            let sindexs = sessionSelectionModel.selectedIndexes
            for(let i = 0; i< sindexs.length; i++) {
                sessionModel.set(sindexs[i], flag, "flagColourRole")
                if(flag_text)
                    sessionModel.set(sindexs[i], flag_text, "flagTextRole")
            }
        }

        function newRecentPath(path) {
            let old = preferences.recent_history.value

            if(old == undefined || !old.length){
                old = Array()
            }

            if(old.length) {
                // remove duplicate
                old = old.filter(function(value, index, arr){
                    return value != path;
                });
            }
            // add to top
            old.unshift(path)

            //prune old entries
            if(old.length > 10)
                old.pop(old.length - 10)

            preferences.recent_history.value = old
        }

        function importSession() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsImportSessionDialog.qml")
            dialog.open()
        }

        function exportNotesCSV() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsExportCSV.qml", sessionFunction)
            dialog.saved.connect(function(path) {
                Future.promise(
                    bookmarkModel.exportCSVFuture(path)
                ).then(function(result) {
                    var msg = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                    msg.title = "Export CSV"
                    msg.text = result
                    msg.show()
                })
            })

            dialog.open()
        }

        function addNote(owner_uuid=null) {
            let uuid = null
            if(bookmarkModel.insertRows(bookmarkModel.rowCount(), 1)) {
                // set owner..
                let ind = bookmarkModel.index(bookmarkModel.rowCount()-1, 0)
                uuid = bookmarkModel.get(ind,"uuidRole")

                if(owner_uuid) {
                    bookmarkModel.set(ind, owner_uuid, "ownerRole")
                } else {
                    bookmarkModel.set(ind, viewport.playhead.mediaSecond, "startRole")
                    bookmarkModel.set(ind, mediaImageSource.fileName, "subjectRole")
                    bookmarkModel.set(ind, 0, "durationRole")
                    bookmarkModel.set(ind, viewport.playhead.mediaUuid, "ownerRole")
                    bookmarkModel.set(ind, preferences.note_category.value, "categoryRole")
                    bookmarkModel.set(ind, preferences.note_colour.value, "colourRole")
                }
            }
            return uuid;
        }

        function closeSession() {
            sessionFunction.checked_unsaved_session = true
            app_window.close()
        }

        function revealSelectedSources() {
            helpers.ShowURIS(copyMediaFileUrl())
        }

        function newSession() {
            studio.newSession("New Session")
            studio.clearImageCache()
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

        function removeSelectedMedia() {
            let items = XsUtils.cloneArray(mediaSelectionModel.selectedIndexes).sort((a,b) => b.row - a.row )

            setActiveMedia(mediaIndexAfterRemoved(items))

            items.forEach(function (item, index) {
                item.model.removeRows(item.row, 1, false, item.parent)
            })
        }

        function gatherMediaForSelected() {
            mediaSelectionModel.model.gatherMediaFor(currentSource.index, mediaSelectionModel.selectedIndexes)
        }

        function relinkSelectedMedia() {
            relink_media_dialog.open()
        }

        function decomposeSelectedMedia() {
            sessionModel.decomposeMedia(mediaSelectionModel.selectedIndexes)
        }

        function rescanSelectedMedia() {
            sessionModel.rescanMedia(mediaSelectionModel.selectedIndexes)
        }

        function requestRemoveSelectedMedia() {
            remove_media.open()
        }

        function saveSessionPath(path) {
            return Future.promise(sessionModel.saveFuture(path))
        }

        function saveSelectedSessionDialog() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSelectedSessionDialog.qml")
            dialog.open()
        }


        function saveSelectedSession(path) {
            return Future.promise(sessionModel.saveFuture(path, sessionSelectionModel.selectedIndexes))
        }

        function saveSessionAs() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSessionDialog.qml")
            dialog.open()
        }

        function saveSession() {
            // console.log("saveSession", sessionPath, app_window.sessionPath)
            if(sessionPath != undefined && sessionPath != "") {
                if(sessionMTime.getTime() != helpers.getFileMTime(sessionPath).getTime()) {
                    overwriteDialog.open()
                } else {
                    sessionFunction.saveSessionPath(sessionPath).then(function(result){
                        if (result != "") {
                            var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                            dialog.title = "Save session failed"
                            dialog.text = result
                            dialog.show()
                        } else {
                            sessionFunction.newRecentPath(sessionPath)
                        }
                    })
                }
            }
            else {
                sessionFunction.saveSessionAs()
            }
        }

        function openSession() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsOpenSessionDialog.qml")
            dialog.open()
        }

        function openRecentCheck(path="") {
            // console.log("openRecentCheck", path)
            if(path == "") {
                sessionFunction.openSession()
            }
            else {
                Future.promise(studio.loadSessionFuture(path)).then(function(result){})
                sessionFunction.newRecentPath(path)
            }
        }

        function saveBeforeOpen(path="") {
            // console.log("saveBeforeOpen", path)

            if(app_window.sessionModel.modified) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.comment = "Save Session Before Opening?"
                dialog.action_text = "Save And Open"
                dialog.saved.connect(function () { openRecentCheck(path)} )
                dialog.dont_save.connect(function () { openRecentCheck(path)} )
                dialog.show()
            } else {
                openRecentCheck(path)
            }
        }


        function saveBeforeClose() {
            if(!app_window.sessionFunction.checked_unsaved_session) {
                // spawn dialog
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.comment = "Save Session Before Closing?"
                dialog.action_text = "Save And Close"
                dialog.saved.connect(app_window.close)
                dialog.dont_save.connect(closeSession)
                dialog.show()
                return false;
            }

            return true;
        }

        function saveBeforeNewSession() {
            if(app_window.sessionModel.modified) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.title = "New Session"
                dialog.comment = "This Session has been modified. Would you like to save it??"
                dialog.action_text = "Save And New Session"
                dialog.saved.connect(newSession)
                dialog.dont_save.connect(newSession)
                dialog.show()
            } else {
                sessionFunction.newSession()
            }
        }

        function copySessionLink(check_saved=true) {
            if(check_saved && app_window.sessionModel.modified) {
                sessionFunction.saveSession()
            }
            clipboard.text = preferences.session_link_prefix.value + "xstudio://open_session?path=" + sessionPath
        }


        function flagSelectedMedia(flag, flag_text="") {
            let sindexs = mediaSelectionModel.selectedIndexes
            for(let i = 0; i< sindexs.length; i++) {
                sessionModel.set(sindexs[i], flag, "flagColourRole")
                if(flag_text)
                    sessionModel.set(sindexs[i], flag_text, "flagTextRole")
            }
        }

        function sortAlphabetically() {
            if(app_window.sessionSelectionModel.currentIndex.valid) {
                app_window.sessionSelectionModel.currentIndex.model.sortAlphabetically(app_window.sessionSelectionModel.currentIndex)
            }
        }

        function selectAllMedia() {
            let media_parent = currentSource.index.model.index(0, 0, currentSource.index)
            let matches = mediaSelectionModel.model.search_list("Media", "typeRole", media_parent, 0, -1)
            mediaSelectionModel.select(helpers.createItemSelection(matches), ItemSelectionModel.ClearAndSelect)
        }

        function deselectAllMedia() {
            if(screenSource.index == currentSource.index)
                setActiveMedia(mediaSelectionModel.currentIndex)
            else
                setActiveMedia(mediaSelectionModel.selectedIndexes[0])
        }

        function copyMediaFilePath(set_clipboard=false) {
            let result = copyMediaFileUrl()

            for(let i =0;i<result.length;i++) {
                result[i] = helpers.pathFromURL(result[i])
            }

            if(set_clipboard)
                clipboard.text = result.join("\n")

            return result
        }

        function copyMediaFileName(set_clipboard=false) {
            let result = copyMediaFilePath()

            for(let i =0;i<result.length;i++) {
                result[i] = result[i].substr(result[i].lastIndexOf("/")+1)
            }

            if(set_clipboard)
                clipboard.text = result.join("\n")

            return result
        }

        function copyMediaFileUrl(set_clipboard=false) {
            let result = []
            let sel = mediaSelectionModel.selectedIndexes
            // path is stored on the current image source.

            for(let i =0;i<sel.length;i++) {
                let mi = sel[i]
                let ms = mi.model.search_recursive(mi.model.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)
                result.push(mi.model.get(ms, "pathRole"))
            }

            if(set_clipboard)
                clipboard.text = result.join("\n")
            return result;
        }


        function conformInsertSelectedMedia(item) {
            sessionModel.conformInsert(item, mediaSelectionModel.selectedIndexes)
        }

        function duplicateSelectedMedia() {
            var media = XsUtils.cloneArray(mediaSelectionModel.selectedIndexes)
            media.forEach(
                (element) => {
                    element.model.duplicateRows(element.row, 1, element.parent)
                }
            )
        }

        function copyMediaToLink() {
            let tmp = app_window.sessionSelectionModel.currentIndex
            if(tmp.valid) {
                let name = tmp.model.get(tmp, "nameRole")
                var filenames = copyMediaFilePath()
                let prefix = "&"+ encodeURIComponent(name)+"_media="

                clipboard.text = preferences.session_link_prefix.value + "xstudio://add_media?compare="+encodeURIComponent(playhead_attrs.compare)+"&playlist=" + encodeURIComponent(name) + prefix + filenames.join(prefix)
            }
        }

        function copyMediaToCmd() {
            // get plalist name.
            let tmp = app_window.sessionSelectionModel.currentIndex
            if(tmp.valid) {

                let name = tmp.model.get(tmp, "nameRole")
                var filenames = copyMediaFilePath()
                let prefix = "&"+ encodeURIComponent(name)+"_media="

                clipboard.text = "xstudio 'xstudio://add_media?compare="+encodeURIComponent(playhead_attrs.compare)+"&playlist=" + encodeURIComponent(name) + prefix + filenames.join(prefix)+"'"
            }
        }

        function clearMediaFromCache() {
            Future.promise(sessionModel.clearCacheFuture(mediaSelectionModel.selectedIndexes)).then(function(result){})
        }

        function setMediaRate(value) {
            var media = mediaSelectionModel.selectedIndexes
            media.forEach(
                (element) => {
                    let mi = element.model.search_recursive(element.model.get(element, "imageActorUuidRole"), "actorUuidRole", element)
                    mi.model.set(mi, value, "rateFPSRole")
                }
            )
        }

        function setMediaRateRequest() {
            var media = mediaSelectionModel.selectedIndexes

            if(media.length) {
                // get first media object -> source -> rate
                let mi = media[0]
                let ms = mi.model.search_recursive(mi.model.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)
                request_media_rate.text = mi.model.get(ms, "rateFPSRole")
                request_media_rate.open()
            }
        }

        function addMediaToNewPlaylist() {
            add_media_to_playlist.open()
        }

        function importSequenceRequest() {
            import_sequence_dialog.open()
        }

        function addMediaToNewSubset() {
            add_media_to_subset.open()
        }

        function addMediaToNewTimeline() {
            add_media_to_timeline.open()
        }
    }

    Connections {
        target: studio

        // function onNewSessionCreated(session_addr) {
        //     console.log("onNewSession", session_addr)
        //     session.sessionActorAddr = session_addr
        // }

        // function onSessionLoaded(session_addr) {
        //     console.log("onSessionLoaded", session_addr)
        //     session.sessionActorAddr = session_addr
        // }

        function onOpenQuickViewers(media_actors, compare_mode) {
            launchQuickViewer(media_actors, compare_mode)
        }

        function onSessionRequest(path, jsn) {
            // console.log("onSessionRequest")
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSessionRequestDialog.qml")
            dialog.path = path
            dialog.payload = jsn
            dialog.show()
        }
        
        function onShowMessageBox(title, body, closeButton, timeoutSecs) {
            messageBox.title = title
            messageBox.text = body
            messageBox.buttonModel = closeButton ? ["Close"] : []
            messageBox.hideTimer(timeoutSecs)
            messageBox.show()
        }
    }

    XsButtonDialog {
        id: messageBox
        // parent: sessionWidget
        width: 400
        onSelected: {
            if(button_index == 0) {
                hide()
            }
        }

        Timer {
            id: hide_timer
            repeat: false
            interval: 500
            onTriggered: messageBox.hide()
        }

        function hideTimer(seconds) {
            if (seconds != 0) {
                hide_timer.interval = seconds*1000
                hide_timer.start()
            }
        }
    
    }

    // Session {
    //     id: session
    // }

    // property alias session: session

    XsSessionWidget {
        id: sessionWidget
        window_name: "main_window"
        is_main_window: true
        focus: true
        Keys.forwardTo: viewport
    }

    property alias sessionWidget: sessionWidget

    property var playerWidget: sessionWidget.playerWidget
    property var hotKeyManager: sessionWidget.playerWidget.shortcuts
    property var viewport: sessionWidget.playerWidget.viewport

    onClosing: {
        semantic_version.store()
        // check for dirty session..

        if(app_window.sessionModel.modified && preferences.check_unsaved_session.value) {
            close.accepted = app_window.sessionFunction.saveBeforeClose()
        }

        if(close.accepted) {
            // First time we start the closedown anim and ignore
            // the close message. The anim then re-sends the close
            // signal, we enter this function again, and we accept it.
            if (sessionWidget.start_closedown_anim()) {
                close.accepted = false
            }
        }
    }

    property var snapshotDialog: undefined

    function toggleSnapshotDialog() {
        if (!snapshotDialog) {
            snapshotDialog = XsUtils.openDialog("qrc:/dialogs/XsSnapshotDialog.qml")
        }
        snapshotDialog.open()
    }

    XsPlayerWindow {
        id: popout_window
        visible: false
        mediaImageSource: app_window.mediaImageSource
        Component.onCompleted: {
            popout_window.viewport.linkToViewport(app_window.viewport)
        }
    }
    property alias popout_window: popout_window

}

