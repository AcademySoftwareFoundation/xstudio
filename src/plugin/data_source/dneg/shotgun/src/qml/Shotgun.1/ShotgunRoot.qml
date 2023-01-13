// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0
import xstudio.qml.uuid 1.0
import xstudio.qml.module 1.0
import xstudio.qml.properties 1.0
import xstudio.qml.viewport 1.0
import Shotgun 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import QtQuick.Controls.impl 2.4
import QtQuick.Templates 2.4 as T

Item {
    id: control
    property bool connected: data_source ? data_source.connected : false
    property alias data_source: data_source

    property var browser: browser

    ShotgunDataSource {
        id: data_source
        name: "testing"
        backendId: control.parent.backendId
        onRequestSecret: do_authentication(message)

        onProjectChanged: {
           if(!data_source.connected) {
                data_source.connected = true
                connection_delay_timer.setTimeout(function(){
                    let ind = data_source.termModels.projectModel.search(project_id, "idRole")
                    if(browser.projectCurrentIndex != ind) {
                        browser.projectCurrentIndex = ind
                    }
                }, 1000);
            } else {
                browser.liveLinkProjectChange = true
                let ind = data_source.termModels.projectModel.search(project_id, "idRole")
                if(browser.projectCurrentIndex != ind) {
                    browser.projectCurrentIndex = ind
                }
            }
        }
    }

    ShotgunAuthenticate {
        width: 400
        id: request_password

        onAccepted: data_source.authenticate(false)
        onRejected: data_source.authenticate(true)
    }

    XsErrorMessage {
        id: error
        // parent: playlist_panel
        x: Math.max(0,(playlist_panel.width - width) / 2)
        y: (playlist_panel.height - height) / 2
        title: "Shotgun datasource error"
    }

    Timer {
        id: timer
    }

    Timer {
        interval: 500
        running: true
        repeat: false

        onTriggered: {
            if(!sessionWidget.is_main_window)
                return

            session.getPlaylists().forEach(
                function (item, index) {
                    processPlaylist(item)
                }
            )
            for(let i=0;i<session.bookmarks.bookmarkModel.length;i++){
                if(session.bookmarks.getJSON(session.bookmarks.bookmarkModel.get(i).objectRole, "/metadata/shotgun")) {
                    addDecorator(session.bookmarks.bookmarkModel.get(i).uuidRole)
                }
            }
            // tree_test.show()
        }
    }

    XsHotkey {
        sequence: "s"
        name: "Show Shot Browser"
        description: "Shows the shot browser interface"
        onActivated: {
            control.shotgun_menu_action("shotgun_browser","shotgun_browser")
        }
    }

    XsModuleAttributes {
        id: attrs_values
        attributesGroupName: "shotgun_datasource_preference"
    }

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupName: "playhead"
    }

    XsModuleAttributes {
        id: shotgun_publish_menu
        attributesGroupName: "shotgun_datasource_menu"
        onValueChanged: control.shotgun_menu_action(key, value)
    }

    XsPreferenceSet {
        id: project_id_pref
        preferencePath: "/plugin/data_source/shotgun/project_id"
    }
    XsPreferenceSet {
        id: location_pref
        preferencePath: "/plugin/data_source/shotgun/location"
    }

    Component.onCompleted: {
        session.object_map["ShotgunRoot"] = control
    }

    XsMenuItem {
        visible: false
        id: toggleBrowser
        onTriggered: toggle_browser()
        shortcut : "S"
        mytext : "Shot Browser..."
    }

    XsMenu {
        visible: false
        id: compareMenu
        title: "Compare With"
        fakeDisabled: false

        Repeater {
            model: data_source.presetModels.mediaActionPresetsModel
            XsMenuItem {
                mytext : nameRole
                onTriggered: shotgun_menu_action("compare_with", nameRole)
            }
        }
    }
    XsMenu {
        visible: false
        id: substituteMenu
        title: "Replace With"
        fakeDisabled: false

        Repeater {
            model: data_source.presetModels.mediaActionPresetsModel
            XsMenuItem {
                mytext : nameRole
                onTriggered: shotgun_menu_action("substitute_with", nameRole)
            }
        }
    }
    XsMenuItem {
        visible: false
        id: allVersionsMenu
        mytext : "Live Versions (All)..."
        onTriggered: shotgun_menu_action("live_all_versions", "")
    }
    XsMenuItem {
        visible: false
        id: relatedMenu
        mytext : "Live Versions (Related)..."
        onTriggered: shotgun_menu_action("live_related_versions", "")
    }
    XsMenuItem {
        visible: false
        id: showNoteseMenu
        mytext : "Live Notes..."
        onTriggered: shotgun_menu_action("notes_history", "")
    }
    XsMenuSeparator {
        visible: false
        id: menuSep
    }

    XsMenu {
        visible: false

        id: compareMenuP
        title: "Compare With"
        fakeDisabled: false

        Repeater {
            model: data_source.presetModels.mediaActionPresetsModel
            XsMenuItem {
                mytext : nameRole
                onTriggered: shotgun_menu_action("compare_with", nameRole)
            }
        }
    }
    XsMenu {
        visible: false
        id: substituteMenuP
        title: "Replace With"
        fakeDisabled: false

        Repeater {
            model: data_source.presetModels.mediaActionPresetsModel
            XsMenuItem {
                mytext : nameRole
                onTriggered: shotgun_menu_action("substitute_with", nameRole)
            }
        }
    }
    XsMenuItem {
        visible: false
        id: allVersionsMenuP
        mytext : "Live Versions (All)..."
        onTriggered: shotgun_menu_action("live_all_versions", "")
    }
    XsMenuItem {
        visible: false
        id: relatedMenuP
        mytext : "Live Versions (Related)..."
        onTriggered: shotgun_menu_action("live_related_versions", "")
    }
    XsMenuItem {
        visible: false
        id: showNoteseMenuP
        mytext : "Live Notes..."
        onTriggered: shotgun_menu_action("notes_history", "")
    }
    XsMenuSeparator {
        visible: false
        id: menuSepP
    }


    Timer {
        running: true
        interval: 1000
        onTriggered: {
            sessionWidget.sessionMenu.panelMenu.insertItem(5, toggleBrowser)

            let m_index = 12
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, showNoteseMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, allVersionsMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, relatedMenu)
            sessionWidget.sessionMenu.mediaMenu.insertMenu(m_index, substituteMenu)
            sessionWidget.sessionMenu.mediaMenu.insertMenu(m_index, compareMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, menuSep)


            sessionWidget.mediaMenu1.insertItem(m_index, showNoteseMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, allVersionsMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, relatedMenuP)
            sessionWidget.mediaMenu1.insertMenu(m_index, substituteMenuP)
            sessionWidget.mediaMenu1.insertMenu(m_index, compareMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, menuSepP)

            toggleBrowser.visible = true
            showNoteseMenu.visible = true
            allVersionsMenu.visible = true
            relatedMenu.visible = true
            menuSep.visible = true
            showNoteseMenuP.visible = true
            allVersionsMenuP.visible = true
            relatedMenuP.visible = true
            menuSepP.visible = true
        }
    }

    Component.onDestruction: {
        sessionWidget.sessionMenu.panelMenu.removeItem(toggleBrowser)

        sessionWidget.sessionMenu.mediaMenu.removeItem(showNoteseMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(allVersionsMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(relatedMenu)
        sessionWidget.sessionMenu.mediaMenu.removeMenu(substituteMenu)
        sessionWidget.sessionMenu.mediaMenu.removeMenu(compareMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(menuSep)


        sessionWidget.mediaMenu1.removeItem(showNoteseMenuP)
        sessionWidget.mediaMenu1.removeItem(allVersionsMenuP)
        sessionWidget.mediaMenu1.removeItem(relatedMenuP)
        sessionWidget.mediaMenu1.removeMenu(substituteMenuP)
        sessionWidget.mediaMenu1.removeMenu(compareMenuP)
        sessionWidget.mediaMenu1.removeItem(menuSepP)
    }


    Connections {
        target: session
        function onBackendChanged() {
            if(!sessionWidget.is_main_window)
                return
            session.getPlaylists().forEach(
                function (item, index) {
                    processPlaylist(item)
                }
            )
        }
        function onNewItem(uuid) {
            // console.log("onNewItem", is_main_window, session, uuid)
            if(!sessionWidget.is_main_window)
                return
            var playlist = session.getPlaylist(uuid)
            if(playlist) {
                processPlaylist(playlist)
            }
        }
    }

    Connections {
        target: session.bookmarks
        function onBackendChanged() {
            if(!sessionWidget.is_main_window)
                return
            for(let i=0;i<session.bookmarks.bookmarkModel.length;i++){
                if(session.bookmarks.getJSON(session.bookmarks.bookmarkModel.get(i).objectRole, "/metadata/shotgun")) {
                    addDecorator(session.bookmarks.bookmarkModel.get(i).uuidRole)
                }
            }
        }
        function onNewBookmark(bookmark_uuid) {
            // console.log(bookmark_uuid)
        }
    }

    Connections {
        target: session.selectedSource ? session.selectedSource.playhead : null
        function onMediaChanged() {
            if(browser.visible) {
                data_source.liveLinkMetadata = session.selectedSource.playhead.media.getMetadata()
            }
        }
    }
    Connections {
        target: session.selectedSource ? session.selectedSource.selectionFilter : null
        function onSelectedMediaUuidsChanged() {
            if(browser.visible &&  session.selectedSource.selectionFilter.selectedMediaUuids.length == 1) {
                let media = session.selectedSource.findMediaObject(session.selectedSource.selectionFilter.selectedMediaUuids[0])
                if(media) {
                    data_source.liveLinkMetadata = media.getMetadata()
                }
            }
        }
    }

    Connections {
        target: browser
        function onVisibleChanged() {
            if(browser.visible) {
                if(session.selectedSource) {
                    data_source.liveLinkMetadata = session.selectedSource.playhead.media.getMetadata()
                }
            }
        }
    }

    ShotgunPreferences {
        id: prefs
        // parent: playlist_panel
        x: Math.max(0,(playlist_panel.width - width) / 2)
        y: (playlist_panel.height - height) / 2
        width: 350
        height: 200
        onAccepted: data_source.connected = true
    }

    TreeTest {
        id: tree_test
        model: data_source.connected ? data_source.sequenceTreeModel(project_id_pref.properties.value) : null
    }

    ShotgunCreatePlaylist {
        id: create_dialog
        width: 350
        height: 200

        onAccepted: create_playlist_promise(data_source, playlist, playlist_uuid, project_id, playlist_name, create_dialog.site_name, error)
    }

    ShotgunPublishNotes {
        id: push_notes_dialog

        projectModel: data_source.termModels.projectModel
        groups: data_source.connected ? data_source.groups : null
        data_source: data_source
        playlists: session.playlistNames
        publish_func: publish_notes
    }

    ShotgunUpdatePlaylist {
        id: update_dialog
        width: 350
        height: 150

        onAccepted: update_playlist_promise(data_source, playlist_uuid, playlist_name, error)
    }

    ShotgunBrowserDialog {
        id: browser

        authenticateFunc: do_authentication
        primaryLocationModel: data_source.termModels.primaryLocationModel
        orderByModel: data_source.termModels.orderByModel
        resultLimitModel: data_source.termModels.resultLimitModel
        boolModel: data_source.termModels.boolModel
        lookbackModel: data_source.termModels.lookbackModel
        stepModel: data_source.termModels.stepModel
        onDiskModel: data_source.termModels.onDiskModel
        twigTypeCodeModel: data_source.termModels.twigTypeCodeModel

        authorModel: data_source.connected ? data_source.termModels.userModel : null
        departmentModel: data_source.connected ? data_source.termModels.departmentModel : null
        noteTypeModel: data_source.connected ? data_source.termModels.noteTypeModel : null
        pipelineStatusModel:  data_source.connected ? data_source.termModels.pipelineStatusModel : null
        playlistTypeModel: data_source.connected ? data_source.termModels.playlistTypeModel : null
        productionStatusModel:  data_source.connected ? data_source.termModels.productionStatusModel : null
        projectCurrentIndex: data_source.termModels.projectModel && data_source.termModels.projectModel.count ? data_source.termModels.projectModel.search(project_id_pref.properties.value, "idRole") : -1
        projectModel: data_source.connected ? data_source.termModels.projectModel : null
        siteModel: data_source.connected ? data_source.termModels.locationModel : null
        shotStatusModel: data_source.connected ? data_source.termModels.shotStatusModel : null

        sequenceTreeModelFunc: data_source.connected ? data_source.sequenceTreeModel : null
        sequenceModelFunc: data_source.connected ? data_source.sequenceModel : null
        shotModelFunc: data_source.connected ? data_source.shotModel : null
        shotSearchFilterModelFunc: data_source.connected ? data_source.shotSearchFilterModel : null
        playlistModelFunc: data_source.connected ? data_source.playlistModel : null

        shotResultsModel: data_source.resultModels.shotResultsModel
        playlistResultsModel: data_source.resultModels.playlistResultsModel
        editResultsModel: data_source.resultModels.editResultsModel
        referenceResultsModel: data_source.resultModels.referenceResultsModel
        noteResultsModel: data_source.resultModels.noteResultsModel
        mediaActionResultsModel: data_source.resultModels.mediaActionResultsModel

        shotPresetsModel: data_source.presetModels.shotPresetsModel
        playlistPresetsModel: data_source.presetModels.playlistPresetsModel
        editPresetsModel: data_source.presetModels.editPresetsModel
        referencePresetsModel: data_source.presetModels.referencePresetsModel
        notePresetsModel: data_source.presetModels.notePresetsModel
        mediaActionPresetsModel: data_source.presetModels.mediaActionPresetsModel

        shotFilterModel: data_source.presetModels.shotFilterModel
        playlistFilterModel: data_source.presetModels.playlistFilterModel
        editFilterModel: data_source.presetModels.editFilterModel
        referenceFilterModel: data_source.presetModels.referenceFilterModel
        noteFilterModel: data_source.presetModels.noteFilterModel
        mediaActionFilterModel: data_source.presetModels.mediaActionFilterModel

        // onVisibleChanged: {
        //     console.log("onVisibleChanged", project_id_pref.properties.value, data_source.projectModel.count, data_source.projectModel.search(project_id_pref.properties.value, "idRole"))
        //     projectCurrentIndex = projectModel.count ? data_source.projectModel.search(project_id_pref.properties.value, "idRole") : -1
        // }

        loadPlaylists: control.loadPlaylists
        addShotsToNewPlaylist: control.addShotsToNewPlaylist
        addShotsToPlaylist: control.addShotsToPlaylist
        addAndCompareShotsToPlaylist: control.addAndCompareShotsToPlaylist

        preferredVisual: data_source.preferredVisual
        preferredAudio: data_source.preferredAudio
        flagText: data_source.flagText
        flagColour: data_source.flagColour

        executeQueryFunc: data_source.connected ? data_source.executeQuery : null
        mergeQueriesFunc: data_source.connected ? data_source.mergeQueries : null
    }


    function delay(delayTime, cb) {
        timer.interval = delayTime;
        timer.repeat = false;
        timer.triggered.connect(cb);
        timer.start();
    }

    function do_authentication(message=""){
        // might already be closing so wait..
        request_password.message = message

        if(request_password.closing) {
            delay(250, function() {
                request_password.show()
            })
        } else {
            request_password.show()
        }

    }

    Timer {
        id: connection_delay_timer
        function setTimeout(cb, delayTime) {
            connection_delay_timer.interval = delayTime;
            connection_delay_timer.repeat = false;
            connection_delay_timer.triggered.connect(cb);
            connection_delay_timer.triggered.connect(function release () {
                connection_delay_timer.triggered.disconnect(cb); // This is important
                connection_delay_timer.triggered.disconnect(release); // This is important as well
            });
            connection_delay_timer.start();
        }
    }

    function create_playlist(playlist=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ create_playlist(playlist); }, 1000);
            return
        }

        create_dialog.siteModel = data_source.termModels.locationModel
        create_dialog.siteCurrentIndex = data_source.termModels.locationModel.search(location_pref.properties.value)
        create_dialog.projectModel = data_source.termModels.projectModel
        create_dialog.projectCurrentIndex = data_source.termModels.projectModel.search(project_id_pref.properties.value, "idRole")

        if(playlist) {
            create_dialog.playlist = playlist
            create_dialog.playlist_name = playlist.name
            create_dialog.playlist_uuid = playlist.uuid
            get_valid_media_count(create_dialog, data_source, playlist.uuid, null)
            create_dialog.show()
        } else {
            XsUtils.getSelectedUuids(app_window.session).forEach(
                function (item, index) {
                    // get playlist name
                    var playlist = session.getPlaylist(item)
                    create_dialog.playlist = playlist
                    create_dialog.playlist_name = playlist.name
                    create_dialog.playlist_uuid = item
                    get_valid_media_count(create_dialog, data_source, item, null)
                    create_dialog.show()
                }
            )
        }
    }

    function update_playlist(playlist=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ update_playlist(playlist); }, 1000);
            return
        }
        update_playlist_(control.data_source, playlist, control.error)
    }

    function refresh_playlist(playlist=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ refresh_playlist(playlist); }, 1000);
            return
        }

        if(playlist) {
            playlist.isBusy = true

            Future.promise(
                data_source.refreshPlaylistVersionsFuture(playlist.uuid)
            ).then(function(json_string) {
                playlist.isBusy = false
                ShotgunHelpers.handle_response(json_string, "Refresh Playlist", true, playlist.name, error)
            })
        } else {
            XsUtils.getSelectedUuids(app_window.session).forEach(function (item, index) {
                var playlist = session.getPlaylist(item)
                playlist.isBusy = true
                Future.promise(
                    data_source.refreshPlaylistVersionsFuture(item)
                ).then(function(json_string) {
                    playlist.isBusy = false
                    ShotgunHelpers.handle_response(json_string, "Refresh Playlist", true, playlist.name, error)
                })
            });
        }
    }

    function create_playlist_promise(data_source, playlist, playlist_uuid, project_id, name, location, error) {
        Future.promise(
            data_source.createPlaylistFuture(playlist_uuid, project_id, name, location)
        ).then(function(json_string) {
            playlist.isBusy = false
            // load new playlist..
            try {
                var data = JSON.parse(json_string)
                if(data["data"]["id"]){
                    load_playlist(data_source, data["data"]["id"], data["data"]["attributes"]["code"])
                }
            } catch(err) {
                // ShotgunHelpers.handle_response(string, "Shotgun Create Playlist", false, name, error)
            }
            ShotgunHelpers.handle_response(json_string, "Shotgun Create Playlist", false, name, error)
        })
    }

    function do_preference(){
        prefs.open()
    }

    function get_valid_media_count(dialog, data_source, playlist_uuid, error) {
        dialog.linking = true
        Future.promise(
            data_source.getPlaylistLinkMediaFuture(playlist_uuid)
        ).then(function(json_string) {
            // load new playlist..
            try {
                dialog.linking = false

                Future.promise(
                    data_source.getPlaylistValidMediaCountFuture(playlist_uuid)
                ).then(function(json_string) {
                    // load new playlist..
                    try {
                        var data = JSON.parse(json_string)
                        dialog.validMediaCount = data["result"]["valid"]
                        dialog.invalidMediaCount = data["result"]["invalid"]
                    } catch(err) {
                        ShotgunHelpers.handle_response(json_string, "get_valid_media_count", true, "", error)
                    }
                })
            } catch(err) {
                ShotgunHelpers.handle_response(json_string, "get_valid_media_count", true, "", error)
            }
        })
    }

    function addDecorator(uuid) {
        let ui = `
            import xStudio 1.0
            import QtQuick 2.14
            XsLabel {
                anchors.fill: parent
                font.pixelSize: XsStyle.popupControlFontSize*1.2
                verticalAlignment: Text.AlignVCenter
                font.weight: Font.Bold
                color: palette.highlight
                text: "SG"
            }
        `
        session.addTag(uuid, "Decorator", ui, uuid.toString()+"ShotgunDecorator");
    }

    function addMenusFull(uuid) {
        let ui = `
            import xStudio 1.0
            XsMenu {
                title: "Shotgun Playlist"
                XsMenuItem {
                    mytext: qsTr("Create...")
                    onTriggered: session.object_map["ShotgunRoot"].create_playlist(playlist.backend)
                }

                XsMenuItem {
                    mytext: qsTr("Refresh")
                    onTriggered: session.object_map["ShotgunRoot"].refresh_playlist(playlist.backend)
                }

                XsMenuItem {
                    mytext: qsTr("Push Contents To Shotgun...")
                    onTriggered: session.object_map["ShotgunRoot"].update_playlist(playlist.backend)
                }
            }
        `
        session.addTag(uuid, "Menu", ui, uuid.toString()+"ShotgunMenu");
    }

    function addMenusPartial(uuid) {
        let ui = `
            import xStudio 1.0
            XsMenu {
                title: "Shotgun Playlist"
                XsMenuItem {
                    mytext: qsTr("Create...")
                    onTriggered: session.object_map["ShotgunRoot"].create_playlist(playlist.backend)
                }
            }
        `
        session.addTag(uuid, "Menu", ui, uuid.toString()+"ShotgunMenu");
    }

    function processPlaylist(playlist) {
        if(playlist.getJSON("/metadata/shotgun")) {
            addDecorator(playlist.uuid)
            addMenusFull(playlist.uuid)
        } else {
            addMenusPartial(playlist.uuid)
        }
    }


    function push_playlist_note_promise(data_source, payload, playlist_uuid, error) {
        Future.promise(
            data_source.pushPlaylistNotesFuture(payload, playlist_uuid)
        ).then(function(json_string) {
            ShotgunHelpers.handle_response(json_string, "Shotgun Publish Note", false, "", error)
        })
    }

    function update_playlist_promise(data_source, playlist, name, error) {
        playlist.isBusy = true
        Future.promise(
            data_source.updatePlaylistVersionsFuture(playlist)
        ).then(function(json_string) {
            playlist.isBusy = false
            // load new playlist..
            ShotgunHelpers.handle_response(json_string, "Push Contents To Shotgun ", false, name, error)
        })
    }

    function publish_notes(payload, playlist_uuid) {
        push_playlist_note_promise(data_source, payload, playlist_uuid, error)
    }

    function push_playlist_notes(playlist=null) {
        push_playlist_notes_(data_source, playlist, error)
    }

    function push_playlist_notes_(data_source, playlist, error) {
        // notes should mirror site/show settings from playlist.
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ push_playlist_notes_(data_source, playlist, error); }, 1000);
            return
        }

        if(playlist) {
            console.log(playlist)
            push_notes_dialog.playlist_uuid = playlist.uuid
            push_notes_dialog.updatePublish()
            push_notes_dialog.show()
        } else {
            if(XsUtils.getSelectedUuids(app_window.session).length) {
                XsUtils.getSelectedUuids(app_window.session).forEach(
                    function (item, index) {
                        // get playlist name
                        var playlist = session.getPlaylist(item)
                        push_notes_dialog.playlist_uuid = item
                        push_notes_dialog.updatePublish()
                        push_notes_dialog.show()
                    }
                )
            } else {
                try {
                    playlist = session.selectedSource.parent_playlist.uuid
                } catch(err) {
                    try {
                        playlist = session.selectedSource.uuid
                    } catch(err) {
                        playlist = null
                    }
                }
                push_notes_dialog.playlist_uuid = playlist
                push_notes_dialog.updatePublish()
                push_notes_dialog.show()
            }
        }
    }

    function update_playlist_(data_source, playlist, error) {
        if(playlist) {
            update_dialog.playlist = playlist
            update_dialog.playlist_name = playlist.name
            update_dialog.playlist_uuid = playlist.uuid
            get_valid_media_count(update_dialog, data_source, playlist.uuid, null)
            update_dialog.show()
        } else {
            XsUtils.getSelectedUuids(app_window.session).forEach(
                function (item, index) {
                    // get playlist name
                    var playlist = session.getPlaylist(item)
                    update_dialog.playlist = playlist
                    update_dialog.playlist_name = playlist.name
                    update_dialog.playlist_uuid = playlist.quuid
                    get_valid_media_count(update_dialog, data_source, item, null)
                    update_dialog.show()
                }
            )
        }
    }

    function shotgun_menu_action(key, value) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ shotgun_menu_action(key, value); }, 1000);
            return
        }

        if(key == "publish_notes_to_shotgun") {
            push_playlist_notes()
        } else if(key == "notes_history") {
            show_browser()
            setBrowserCategory("Notes")
            createPresetType("Live Notes")
        } else if(key == "live_related_versions") {
            show_browser()
            setBrowserCategory("Shots")
            createPresetType("Live Related Versions")
        } else if(key == "live_all_versions") {
            show_browser()
            setBrowserCategory("Shots")
            createPresetType("Live All Versions")
        } else if(key == "all_versions") {
            show_browser()
            setBrowserCategory("Shots")
            createPresetType("All Versions")
        } else if(key == "substitute_with") {
            substitute_with(value)
        } else if(key == "compare_with") {
            compare_with(value)
        } else if(key == "notes_to_shotgun") {
            push_playlist_notes()
        } else if(key == "shotgun_browser") {
            toggle_browser()
        }
    }

    function substitute_with(preset_name) {
        //for each selected media..
        let selected = session.selectedMediaUuids()

        selected.forEach(
            function (item, index) {
                var source = session.selectedSource ? session.selectedSource : session.onScreenSource
                let media_obj = source.findMediaObject(item)

                if(media_obj) {
                    browser.executeMediaActionQuery(preset_name, media_obj.getMetadata(),
                        function (data) {
                            // load item after existing
                            // delete existing..
                            let result = JSON.parse(
                                    data_source.addVersionToPlaylist(
                                    JSON.stringify(data),
                                    source.uuid,
                                    media_obj.uuid
                                )
                            )
                            // select new item ?
                            if(result.length) {
                            // remove old..
                                let remove_uuid = media_obj.uuid
                                source.removeMedia(remove_uuid)
                                let new_uuids = []
                                for(let i=0;i<result.length; i++) {
                                    new_uuids.push(helpers.QVariantFromUuidString(result[i]))
                                }

                                let cur = session.selectedSource.selectionFilter.currentSelection()
                                for(let i=0;i<cur.length; i++) {
                                    if(cur[i] != remove_uuid) {
                                        new_uuids.push(cur[i])
                                    }
                                }

                                session.selectedSource.selectionFilter.newSelection(
                                    new_uuids
                                )
                            }
                        }
                    )
                }
            }
        )
    }

    function compare_with(preset_name) {
        let selected = session.selectedMediaUuids()
        var source = session.selectedSource ? session.selectedSource : session.onScreenSource
        selected.forEach(
            function (item, index) {
                let media_obj = source.findMediaObject(item)
                if(media_obj) {
                    browser.executeMediaActionQuery(preset_name, media_obj.getMetadata(),
                        function (data) {
                            // load item after existing

                            let result = JSON.parse(
                                    data_source.addVersionToPlaylist(
                                    JSON.stringify(data),
                                    source.uuid,
                                    session.selectedSource.getNextItemUuid(media_obj.uuid)
                                )
                            )
                            // order matters,,

                            if(result.length) {
                                let new_uuids = []
                                new_uuids.push(media_obj.uuid)

                                for(let i=0;i<result.length; i++) {
                                    new_uuids.push(helpers.QVariantFromUuidString(result[i]))
                                }

                                let cur = session.selectedSource.selectionFilter.currentSelection()
                                for(let i=0;i<cur.length; i++) {
                                    new_uuids.push(cur[i])
                                }

                                session.selectedSource.selectionFilter.newSelection(
                                    new_uuids
                                )
                            }
                        }
                    )
                }
            }
        )
        playhead_attrs.compare = "A/B"
    }


    function createPresetType(mode) {
        browser.createPresetType(mode)
    }

    function setBrowserCategory(cat) {
        browser.currentCategory = cat
    }

    function toggle_browser() {
        data_source.connected = true
        browser.toggle()
        if(browser.visible) {
            browser.raise()
            browser.requestActivate()
        }
    }

    function show_browser() {
        data_source.connected = true
        browser.show()
        browser.raise()
    }

    function loadPlaylists(items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        items.forEach(
            function (item, index) {
                load_playlist(data_source, item.id, item.name, error, preferred_visual, preferred_audio, flag_text, flag_colour)
            }
        )
    }

    // items are version metadata from shotgun.
    function addVersionsToPlaylist(uuid, items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        let query = ""

        if(typeof(items) == "string") {
            try {
                let j = JSON.parse(items)
                j["data"]["preferred_visual_source"] = preferred_visual
                j["data"]["preferred_audio_source"] = preferred_audio
                j["data"]["flag_text"] = flag_text
                j["data"]["flag_colour"] = flag_colour
                query = JSON.stringify(j)
            }catch(err){
            }
        } else {
            let versions = [];
            items.forEach(
                function (item, index) {
                    versions.push(JSON.stringify(item.json))
                }
            )
            if(versions.length) {
                query = '{"data":[' + versions.join(",") + '], ' +
                '"preferred_visual_source": "' + preferred_visual + '",' +
                '"preferred_audio_source": "' + preferred_audio + '",' +
                '"flag_text": "' + flag_text + '",' +
                '"flag_colour": "' + flag_colour + '"' +
                '}'
            }
        }

        if(query.length) {
        // add versions to playlist.
            var fa = data_source.addVersionToPlaylistFuture(
                query,
                uuid
            )
            Future.promise(fa).then(
                function(json_string) {
                    try {
                        var data = JSON.parse(json_string)
                        // should be array of uuids..
                        session.switchOnScreenSource(uuid)
                        // turn uuid strings in to uuids..
                        session.setSelection([uuid], true)
                        if(data.length) {
                            let selection = [helpers.QVariantFromUuidString(data[0])]
                            session.selectedSource.selectionFilter.newSelection(selection)
                        }
                        app_window.requestActivate()
                        app_window.raise()
                        sessionWidget.playerWidget.forceActiveFocus()

                    } catch(err) {
                        console.log(err)
                        ShotgunHelpers.handle_response(json_string)
                    }
                },
                function() {
                }
            )
        }
    }


    // button
    function addAndCompareShotsToPlaylist(name, items, mode="A/B", preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {

        let query = ""

        if(typeof(items) == "string") {
            try {
                let j = JSON.parse(items)
                j["data"]["preferred_visual_source"] = preferred_visual
                j["data"]["preferred_audio_source"] = preferred_audio
                j["data"]["flag_text"] = flag_text
                j["data"]["flag_colour"] = flag_colour
                query = JSON.stringify(j)
            }catch(err){
            }
        } else {
            let versions = [];
            items.forEach(
                function (item, index) {
                    versions.push(JSON.stringify(item.json))
                }
            )
            if(versions.length) {
                query = '{"data":[' + versions.join(",") + '], ' +
                '"preferred_visual_source": "' + preferred_visual + '",' +
                '"preferred_audio_source": "' + preferred_audio + '",' +
                '"flag_text": "' + flag_text + '",' +
                '"flag_colour": "' + flag_colour + '"' +
                '}'
            }
        }

        if(query.length) {
            var uuid
            var fa

            if(!session.selectedSource) {
                uuid = session.createPlaylist(name)
                session.switchOnScreenSource(uuid)
                session.setSelection([uuid], true)
            } else {
                uuid = session.selectedSource.uuid
            }

            try {
                let selection = session.selectedSource.selectionFilter.selectedMediaUuids
                fa = data_source.addVersionToPlaylistFuture(
                    query,
                    uuid,
                    session.selectedSource.getNextItemUuid(selection[selection.length-1])
                )
            } catch(err) {
                fa = data_source.addVersionToPlaylistFuture(
                    query,
                    uuid
                )
            }

            // add versions to playlist.

            Future.promise(fa).then(
                function(json_string) {
                    // select media uuids..
                    try {
                        var data = JSON.parse(json_string)
                        // should be array of uuids..
                        session.switchOnScreenSource(uuid)
                        // turn uuid strings in to uuids..
                        let selection = session.selectedSource.selectionFilter.selectedMediaUuids

                        data.forEach(
                            function (item, index) {
                                if(item !== "")
                                    selection.push(helpers.QVariantFromUuidString(item))
                            }
                        )
                        session.setSelection([uuid], true)
                        session.selectedSource.selectionFilter.newSelection(selection)
                        playhead_attrs.compare = mode
                    } catch(err) {
                        console.log(err)
                        ShotgunHelpers.handle_response(json_string)
                    }
                },
                function() {
                }
            )
        }
    }

    function addShotsToNewPlaylist(name, items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        var uuid = session.createPlaylist(name)
        addVersionsToPlaylist(uuid, items, preferred_visual, preferred_audio, flag_text, flag_colour )
    }

    function addShotsToPlaylist(items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        var uuid

        if(!session.selectedSource) {
            uuid = session.createPlaylist("Shot Browser Media")
        } else {
            uuid = session.selectedSource.uuid
        }
        session.switchOnScreenSource(uuid)
        session.setSelection([uuid], true)
        addVersionsToPlaylist(uuid, items, preferred_visual, preferred_audio, flag_text, flag_colour)
    }

    function load_playlist(data_source, id, name, error, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        // busy.running = true
        // create new playlist
        var uuid = session.createPlaylist(name)

        // get playlist json from shotgun
        var ff = data_source.getPlaylistVersionsFuture(id)
        Future.promise(ff).then(function(json_string) {
            try {
                // console.time("load_playlist");
                var data = JSON.parse(json_string)
                if(data["data"]){
                    var playlist = session.getPlaylist(uuid)
                    playlist.isBusy = true
                    playlist.setJSON(JSON.stringify(data['data']), "/metadata/shotgun/playlist")
                    addDecorator(playlist.uuid)
                    addMenusFull(playlist.uuid)

                    data["preferred_visual_source"] = preferred_visual
                    data["preferred_audio_source"] = preferred_audio
                    data["flag_text"] = flag_text
                    data["flag_colour"] = flag_colour

                    // add versions to playlist.
                    var fa = data_source.addVersionToPlaylistFuture(JSON.stringify(data), playlist.uuid)
                    Future.promise(fa).then(
                        function(json_string) {
                            playlist.isBusy = false
                            ShotgunHelpers.handle_response(json_string)
                        },
                        function() {
                            playlist.isBusy = false
                        }
                    )
                } else {
                    error.title = "Load Shotgun Playlist " + name
                    error.text = json_string
                    error.open()
                }
            }
            catch(err) {
                error.title = "Load Shotgun Playlist " + name
                error.text = err + "\n" + json_string
                error.open()
                console.log(err, json_string)
            }
        })
    }
}
