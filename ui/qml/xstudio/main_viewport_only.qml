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


//------------------------------------------------------------------------------
// BEGIN COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------
import xstudio.qml.viewport 1.0
import xstudio.qml.session 1.0
import xstudio.qml.playlist 1.0
import xstudio.qml.semver 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.clipboard 1.0
import xstudio.qml.bookmarks 1.0

//------------------------------------------------------------------------------
// END COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------

import xStudio 1.0

Rectangle {
    id: app_window
    visible: true
    color: "#00000000"
    objectName: "appWindow"
    // override default palette

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
    Clipboard {
      id: clipboard
    }


    SemVer {
        id: semantic_version
        version: Qt.application.version

        function show_features() {
            if (semantic_version.compare(preferences.latest_version.value) > 0) {
                XsUtils.openDialog("qrc:/dialogs/XsFunctionalFeaturesDialog.qml").open()
            }
            if(semantic_version.compare(preferences.latest_version.value) > 0) {
                // immediate update..
                preferences.latest_version.value = Qt.application.version
            }
        }

        function store() {
            if(semantic_version.compare(preferences.latest_version.value) > 0) {
                // immediate update..
                preferences.latest_version.value = Qt.application.version
            }
        }
    }


    XsGlobalPreferences {
        id: preferences
    }
    property alias preferences: preferences

    // the global store object is instantiated and injected into
    // this var by the c++ main method
    BookmarkDetail {
        id: bookmark_detail
        colour: ""
    }

    Session {
        id: session
        property var checked_unsaved_session: false
        // so QML can find their QML instances.
        property var object_map: ({})
        property var bookmark_detail: bookmark_detail

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

        function flag_selected_items(flag) {
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
            var to_remove = []
            XsUtils.forAllItems(session, null, add_selected_item, to_remove)
            to_remove.forEach(removeContainer)
        }

        function open_session() {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsOpenSessionDialog.qml")
            dialog.open()
        }

        function save_before_open() {
            if(modified) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.comment = "Save Session Before Opening?"
                dialog.action_text = "Save And Open"
                dialog.saved.connect(open_session)
                dialog.dont_save.connect(open_session)
                dialog.open()
            } else {
                open_session()
            }
        }

        function save_before_close() {
            if(!checked_unsaved_session) {
                // spawn dialog
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveBeforeDialog.qml")
                dialog.comment = "Save Session Before Closing?"
                dialog.action_text = "Save And Close"
                dialog.saved.connect(app_window.close)
                dialog.dont_save.connect(closeSession)
                dialog.open()
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
                dialog.open()
            } else {
                new_session()
            }
        }

        function save_session(path) {
            var error_msg = save(path)
            if (error_msg != "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Save session failed"
                dialog.text = error_msg
                dialog.show()
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
            }
        }

        function copy_media_file_name() {
            var filenames = selectedSource ? selectedSource.selectionFilter.selectedMediaFileNames() : onScreenSource.selectionFilter.selectedMediaFileNames()
            clipboard.text = filenames.join("\n")
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
                uuid = bookmarkModel.get(ind,"uuidRole")

                if(owner_uuid) {
                    bookmarkModel.set(ind, owner_uuid, "ownerRole")
                }
            }
            return uuid;
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

        function add_media_to_new_subset(source=parent) {
            var media = selectedSource ? selectedSource.selectionFilter.selectedMediaUuids : onScreenSource.selectionFilter.selectedMediaUuids
            var dlg = XsUtils.openDialog("qrc:/dialogs/XsNewSubsetDialog.qml", source)
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
    }
    property alias sessionWidget: sessionWidget


    Component.onCompleted: {
        sessionWidget.layout_name = preferences.enable_presentation_mode.value ? "presentation_layout" : "review_layout"
        sessionWidget.playerWidget.layout_bar_visible = false
        sessionWidget.playerWidget.status_bar_visible = false
        sessionWidget.playerWidget.menu_bar_visible = false
    }

    property var playerWidget: sessionWidget.playerWidget

}

