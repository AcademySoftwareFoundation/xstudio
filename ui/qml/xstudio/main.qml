// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.15
// import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtGraphicalEffects 1.12
import QtQuick.Shapes 1.12
import Qt.labs.platform 1.1
import Qt.labs.settings 1.0
import QtQml.Models 2.15
import QtQml 2.15

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
import xstudio.qml.playlist 1.0
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
    title: (session.pathNative ? (session.modified ? session.pathNative + " - modified": session.pathNative) : "xStudio")
    objectName: "appWidnow"
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

    property var popout_window: undefined

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
                session.load(encodeURIComponent(autosave_path))
            }
        }
    }

    function togglePopoutViewer() {
        if (popout_window) {
            popout_window.toggle_visible()
            return
        }

        var component = Qt.createComponent("player/XsPlayerWindow.qml");
        if (component.status == Component.Ready) {
            popout_window = component.createObject(app_window, {x: 100, y: 100, session: session});
            popout_window.show()
        } else {
            // Error Handling
            console.log("Error loading component:", component.errorString());
        }
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

    function fitWindowToImage() {
        // doesn't apply to session window
        spawnNewViewer()
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
                session.save_session_path(session.path)
            } else if(button_index == 2) {
                session.save_session_as()
            }
        }
    }

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupName: "playhead"
    }

    XsBookmarkModel {
        id: bookmarkModel
        bookmarkActorAddr: session.bookmarkActorAddr
    }


    // XsModelProperty {
    //     id: session_name
    //     role: "nameRole"
    //     index: app_window.sessionModel.index(0,0)
    //     onValueChanged: {
    //         console.log("nameRole", value)
    //     }
    // }

    // Connections {
    //     target: sessionModel
    //     function onSessionActorAddrChanged() {
    //         session_name.index = sessionModel.index(0,0)
    //     }
    // }

    // XsHotkey {
    //     sequence: "Alt+e"
    //     name: "test"
    //     description: "test"
    //     onActivated: {
    //         session_name.value = "Test"
    //     }
    // }

    // property alias sessionModel: sessionModel

    // XsSessionModel {
    //     id: sessionModel
    //     sessionActorAddr: session.sessionActorAddr
    // }

    Session {
        id: session

        property alias bookmarkModel: bookmarkModel
        property var checked_unsaved_session: false
        // so QML can find their QML instances.
        property var object_map: ({})

        onSessionRequest: {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSessionRequestDialog.qml")
            dialog.path = path
            dialog.payload = jsn
            dialog.show()
        }

        function closeSession() {
            app_window.session.checked_unsaved_session = true
            app_window.close()
        }

        function new_session() {
            newSession("New Session")
        }

        function add_selected_item(parent, obj, value) {
            if(obj.selected) {
                value.push(obj.cuuid)
                return true
            }
            return false
        }

        function add_selected_item_uuid(parent, obj, value) {
            if(obj.selected) {
                value.push(obj.uuid)
                return true
            }
            return false
        }

        function flag_selected_items(flag, text) {
            function toggleFlag(parent, obj, value) {
                if(obj.selected && obj.flag != value) {
                    parent.reflagContainer(value, obj.cuuid)
                    return true
                }
                return false
            }
            // multiselect..
            XsUtils.forAllItems(app_window.session, null, toggleFlag, flag)
        }

        function remove_selected_items() {
            // N.B. we need to remove tabs that point to items that are about
            // to be removed, but also change the slection so that a tab that
            // hasn't been deleted becomes current
            var to_remove = []
            var to_remove2 = []
            XsUtils.forAllItems(session, null, add_selected_item, to_remove2)
            XsUtils.forAllItems(session, null, add_selected_item_uuid, to_remove)
            to_remove2.forEach(removeContainer)
        }

        function new_recent_path(path) {
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

        function open_session() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsOpenSessionDialog.qml")
            dialog.open()
        }

        function save_before_open(path="") {
            function open_recent_check() {
                if(path == "") {
                    open_session()
                }
                else {
                    session.load(path)
                    new_recent_path(path)
                }
            }

            if(modified) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.comment = "Save Session Before Opening?"
                dialog.action_text = "Save And Open"
                dialog.saved.connect(open_recent_check)
                dialog.dont_save.connect(open_recent_check)
                dialog.show()
            } else {
                open_recent_check()
            }
        }

        function save_before_close() {
            if(!app_window.session.checked_unsaved_session) {
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

        function save_before_new_session() {
            if(modified) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.title = "New Session"
                dialog.comment = "This Session has been modified. Would you like to save it??"
                dialog.action_text = "Save And New Session"
                dialog.saved.connect(new_session)
                dialog.dont_save.connect(new_session)
                dialog.show()
            } else {
                new_session()
            }
        }

        function import_session() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsImportSessionDialog.qml")
            dialog.open()
        }

        function save_session_as() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSessionDialog.qml")
            dialog.open()
        }

        function copy_session_link(check_saved=true) {
            if(check_saved && app_window.session.modified) {
                save_session()
            }
            clipboard.text = preferences.session_link_prefix.value + "xstudio://open_session?path=" + app_window.session.posixPath()
        }

        function save_session() {
            if(session.pathNative != "") {
                if(session.sessionFileMTime.getTime() != helpers.getFileMTime(session.path).getTime()) {
                    overwriteDialog.open()
                } else {
                    session.save_session_path(session.path)
                }
            }
            else {
                session.save_session_as()
            }
        }


        function save_session_path(path) {
            var error_msg = save(path)
            if (error_msg != "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Save session failed"
                dialog.text = error_msg
                dialog.show()
            } else {
                app_window.session.new_recent_path(path)
            }
            return error_msg;
        }

        function save_selected_session(path) {
            var error_msg = save_selected(path)
            if (error_msg != "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Save selected session failed"
                dialog.text = error_msg
                dialog.show()
            } else {
                app_window.session.new_recent_path(path)
            }
        }


        function add_media_from_clipboard(source=app_window.session) {
            // expects paths in clipboard, possibly multiple
            // can we piggy back on something..
            if(clipboard.text.length) {
                let ct = ""
                clipboard.text.split("\n").forEach(function (item, index) {
                        ct = ct + "file:" + item + "\n"
                    }
                )
                Future.promise(source.handleDropFuture({"text/uri-list": ct})).then(function(quuids){})
            }
        }

        function copy_media_file_name() {
            var filenames = selectedSource ? selectedSource.selectionFilter.selectedMediaFileNames() : onScreenSource.selectionFilter.selectedMediaFileNames()
            clipboard.text = filenames.join("\n")
        }

        function copy_media_to_link() {
            let source = selectedSource ? selectedSource : onScreenSource
            var filenames = source.selectionFilter.selectedMediaFilePaths()
            let prefix = "&"+ encodeURIComponent(source.name)+"_media="

            clipboard.text = preferences.session_link_prefix.value + "xstudio://add_media?compare="+encodeURIComponent(playhead_attrs.compare)+"&playlist=" + encodeURIComponent(source.name) + prefix + filenames.join(prefix)
        }

        function copy_media_to_cmd() {
            let source = selectedSource ? selectedSource : onScreenSource
            var filenames = source.selectionFilter.selectedMediaFilePaths()
            let prefix = "&"+ encodeURIComponent(source.name)+"_media="

            clipboard.text = "xstudio 'xstudio://add_media?compare="+encodeURIComponent(playhead_attrs.compare)+"&playlist=" + encodeURIComponent(source.name) + prefix + filenames.join(prefix)+"'"
        }

        function copy_media_file_path() {
            var filenames = selectedSource ? selectedSource.selectionFilter.selectedMediaFilePaths() : onScreenSource.selectionFilter.selectedMediaFilePaths()
            clipboard.text = filenames.join("\n")
        }

        function reveal_selected_sources() {
            var urls = selectedSource ? selectedSource.selectionFilter.selectedMediaURLs() : onScreenSource.selectionFilter.selectedMediaURLs()
            helpers.ShowURIS(urls)
            // console.log(urls)
        }

        function selectedMediaUuids() {
            var source = selectedSource ? selectedSource : onScreenSource
            return source.selectionFilter.selectedMediaUuids
        }

        function flag_selected_media(flag, text) {
            var source = selectedSource ? selectedSource : onScreenSource
            var media = source.selectionFilter.selectedMediaUuids
            source.mediaList.forEach(
                (element) => {
                    if(media.includes(element.uuid)) {
                        element.flag = flag
                        element.flagText = text
                    }
                }
            )
        }

        function flag_current_media(flag, text) {
            var media = viewport.playhead.media
            media.flag = flag
            media.flagText = text
        }

        function duplicate_selected_media() {
            var source = selectedSource ? selectedSource : onScreenSource
            var media = source.selectionFilter.selectedMediaUuids
            var src_uuid = source.uuid
            media.forEach(
                (element) => {
                    session.copyMedia(src_uuid, [element], true, source.getNextItemUuid(element))
                }
            )
        }

        function add_note(owner_uuid=null) {
            let uuid = null
            if(bookmarkModel.insertRows(bookmarkModel.rowCount(), 1)) {
                // set owner..
                let ind = bookmarkModel.index(bookmarkModel.rowCount()-1, 0)
                bookmarkModel.set(ind, preferences.note_category.value, "categoryRole")
                bookmarkModel.set(ind, preferences.note_colour.value, "colourRole")
                if(owner_uuid) {
                    bookmarkModel.set(ind, owner_uuid, "ownerRole")
                }

                uuid = bookmarkModel.get(ind,"uuidRole")
            }
            return uuid;
        }

        function export_notes_csv() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsExportCSV.qml", session)
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

        function add_media_to_new_playlist(source=parent) {
            var media = selectedSource ? selectedSource.selectionFilter.selectedMediaUuids : onScreenSource.selectionFilter.selectedMediaUuids
            var src_uuid = selectedSource ? selectedSource.uuid : onScreenSource.uuid
            var dlg = XsUtils.openDialog("qrc:/dialogs/XsNewPlaylistDialog.qml", source)
            dlg.okay_text = "Copy Media"
            dlg.secondary_okay_text = "Move Media"

            dlg.created.connect(function(uuid) {session.copyMedia(uuid.asQuuid, media)})
            dlg.created_secondary.connect(function(uuid) {session.moveMedia(uuid.asQuuid, src_uuid, media)})
            dlg.open()
        }

        function set_selected_media_rate(source=parent) {
            var source = selectedSource ? selectedSource : onScreenSource
            var media = source.selectionFilter.selectedMediaUuids

            if(media.length) {
                // get first media object -> source -> rate
                var dlg = XsUtils.openDialog("qrc:/dialogs/XsSetMediaRateDialog.qml", source)
                dlg.okay_text = "Set Media Rate"
                dlg.text = source.findMediaObject(media[0]).mediaSource.fpsString
                dlg.okayed.connect(
                    function() {
                        for(let i=0;i<media.length; i++) {
                            source.findMediaObject(media[0]).mediaSource.fpsString = dlg.text
                        }
                    }
                )
                dlg.open()
            }
        }

        function add_media_to_new_subset(source=parent) {
            var media = selectedSource ? selectedSource.selectionFilter.selectedMediaUuids : onScreenSource.selectionFilter.selectedMediaUuids
            var dlg = XsUtils.openDialog("qrc:/dialogs/XsNewSubsetDialog.qml", source)
            dlg.okay_text = "Add Media"
            dlg.created.connect(function(uuid) {session.copyMedia(uuid.asQuuid, media)})
            dlg.open()
        }

        function add_media_to_new_contact_sheet(source=parent) {
            var media = selectedSource ? selectedSource.selectionFilter.selectedMediaUuids : onScreenSource.selectionFilter.selectedMediaUuids
            var dlg = XsUtils.openDialog("qrc:/dialogs/XsNewContactSheetDialog.qml", source)

            dlg.okay_text = "Add Media"
            dlg.created.connect(function(uuid) {session.copyMedia(uuid.asQuuid, media)})
            dlg.open()
        }
    }

    property alias session: session

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

        if(app_window.session.modified && preferences.check_unsaved_session.value) {
            close.accepted = app_window.session.save_before_close()
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

}

