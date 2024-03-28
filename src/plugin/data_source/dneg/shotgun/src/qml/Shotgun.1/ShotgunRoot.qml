// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.uuid 1.0
import xstudio.qml.module 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import Shotgun 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import QtQuick.Controls.impl 2.4
import QtQuick.Templates 2.4 as T

Item {
    id: control
    property bool connected: data_source ? data_source.connected : false
    property alias data_source: data_source


    readonly property string menuLiveHistory : "Live History"
    readonly property string menuLiveLatest : "Live Latest"
    readonly property string menuLiveNotes : "Live Notes"

    readonly property string menuHistory : "History"
    readonly property string menuLatest : "Latest"

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
        title: "ShotGrid datasource error"
    }

    XsStringRequestDialog {
        id: create_ref_tag_dialog
        okay_text: "Create Reference Tag"
        text: "Tag"
        onOkayed: {
            Future.promise(
                data_source.createTagFuture(text.toUpperCase()+".REFERENCE")
            ).then(function(json_string) {
                // console.log(json_string)
            })
        }
    }

    XsStringRequestDialog {
        id: rename_ref_tag_dialog
        okay_text: "Rename Reference Tag"
        text: "Tag"
        property int tag_id: 0
        onOkayed: {
            Future.promise(
                data_source.renameTagFuture(tag_id, text.toUpperCase()+".REFERENCE")
            ).then(function(json_string) {
                // console.log(json_string)
            })
        }
    }

    ShotgunTagDialog {
        id: tag_dialog
        data_source: control.data_source
        tagMethod: control.tagSelectedMedia
        untagMethod: control.untagSelectedMedia
        newTagMethod: create_ref_tag_dialog.open

        renameTagMethod: control.renameTag
        removeTagMethod: control.removeTag

        onVisibleChanged: {
            if(!data_source.connected && visible)
                data_source.connected = true
        }
    }


    Timer {
        id: timer
    }

    Timer {
        id: tag_timer
        interval: 2000
        running: true
        repeat: false

        onTriggered: {
            if(!sessionWidget.is_main_window || !app_window.bookmarkModel)
                return

            for(let i=0;i<app_window.bookmarkModel.length;i++){
                let ind = app_window.bookmarkModel.index(i, 0)
                if(ind.valid && app_window.bookmarkModel.getJSON(ind, "/metadata/shotgun")) {
                    addDecorator(app_window.bookmarkModel.get(ind, "uuidRole"))
                }
            }
        }
    }

    XsHotkey {
        sequence: "Shift+T"
        name: "Show Reference Tag"
        description: "Shows the tag browser interface"
        onActivated: {
            tag_dialog.toggle()
        }
    }

    XsHotkey {
        sequence: "s"
        name: "Show Shot Browser"
        description: "Shows the shot browser interface"
        onActivated: {
            control.shotgun_menu_action("shotgun_browser", "shotgun_browser")
        }
    }

    XsHotkey {
        sequence: "Alt+n"
        name: "Next Version"
        description: "Replace With Next Version"
        onActivated: {
            control.shotgun_menu_action("substitute_with", "Next Version")
        }
    }

    XsHotkey {
        sequence: "Alt+p"
        name: "Previous Version"
        description: "Replace With Previous Version"
        onActivated: {
            control.shotgun_menu_action("substitute_with", "Previous Version")
        }
    }

    XsHotkey {
        sequence: "Alt+l"
        name: "Latest Version"
        description: "Replace With Latest Version"
        onActivated: {
            control.shotgun_menu_action("substitute_with", "Latest Version")
        }
    }

    XsModuleAttributes {
        id: attrs_values
        attributesGroupNames: "shotgun_datasource_preference"
    }

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupNames: "playhead"
    }

    XsModuleAttributes {
        id: shotgun_publish_menu
        attributesGroupNames: "shotgun_datasource_menu"
        onValueChanged: control.shotgun_menu_action(key, value)
    }

    XsModelProperty {
        id: project_id_preference
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/plugin/data_source/shotgun/project_id", "pathRole")
    }

    property alias currentCategory: context_preference.value

    XsModelProperty {
        id: context_preference
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/plugin/data_source/shotgun/context", "pathRole")
    }

    XsModelProperty {
        id: location_preference
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/plugin/data_source/shotgun/location", "pathRole")
    }

    Component.onCompleted: {
        sessionFunction.object_map["ShotgunRoot"] = control
    }

    XsMenuItem {
        visible: false
        id: toggleBrowser
        onTriggered: toggle_browser()
        shortcut : "S"
        mytext : "Shot Browser..."
    }

    XsMenuItem {
        visible: false
        id: revealInShotgunMenu
        onTriggered: revealInShotgun()
        mytext : "Reveal In ShotGrid..."
    }

    XsMenuItem {
        visible: false
        id: revealInIvyMenu
        onTriggered: revealInIvy()
        mytext : "Reveal In Ivy..."
    }

    XsMenuItem {
        visible: false
        id: downloadMovieMenu
        mytext :  "Download SG Movie"
        onTriggered: shotgun_menu_action("download_movie", app_window.mediaSelectionModel.selectedIndexes)
    }

    XsMenuItem {
        visible: false
        id: downloadMissingMovieMenu
        mytext :  "Download SG Previews (missing)"
        onTriggered: shotgun_menu_action("download_missing_movie", app_window.currentSource.index)
    }

    XsMenuItem {
        visible: false
        id: referenceMenu
        shortcut : "Shift+T"
        mytext: "Reference Tag (Experimental)"
        onTriggered: tag_dialog.toggle()
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
        id: replaceNextMenu
        mytext :  "Next Version"
        shortcut : "Alt+N"
        onTriggered: shotgun_menu_action("substitute_with", "Next Version")
    }
    XsMenuItem {
        visible: false
        id: replacePreviousMenu
        mytext :  "Previous Version"
        shortcut : "Alt+P"
        onTriggered: shotgun_menu_action("substitute_with", "Previous Version")
    }
    XsMenuItem {
        visible: false
        id: replaceLatestMenu
        mytext :  "Latest Version"
        shortcut : "Alt+l"
        onTriggered: shotgun_menu_action("substitute_with", "Latest Version")
    }

    XsMenuItem {
        visible: false
        id: allVersionsMenu
        // mytext : menuLiveLatest + "..."
        mytext : "Shot Browser Shot..."
        onTriggered: shotgun_menu_action("live_latest_versions", "")
    }
    XsMenuItem {
        visible: false
        id: relatedMenu
        mytext : "Shot Browser Version Stream..."
        // mytext : menuLiveHistory + "..."
        onTriggered: shotgun_menu_action("live_version_history", "")
    }
    XsMenuItem {
        visible: false
        id: showNoteseMenu
        mytext : "Shot Browser Notes..."
        // mytext : menuLiveNotes + "..."
        onTriggered: shotgun_menu_action("notes_history", "")
    }
    XsMenuSeparator {
        visible: false
        id: menuSep
    }

    XsMenuItem {
        visible: false
        id: revealInShotgunMenuP
        onTriggered: revealInShotgun()
        mytext : "Reveal In ShotGrid..."
    }

    XsMenuItem {
        visible: false
        id: revealInIvyMenuP
        onTriggered: revealInIvy()
        mytext : "Reveal In Ivy..."
    }

    XsMenuItem {
        visible: false
        id: downloadMovieMenuP
        mytext :  "Download SG Movie"
        onTriggered: shotgun_menu_action("download_movie", app_window.mediaSelectionModel.selectedIndexes)
    }

    XsMenuItem {
        visible: false
        id: downloadMissingMovieMenuP
        mytext :  "Download SG Previews (missing)"
        onTriggered: shotgun_menu_action("download_missing_movie", app_window.currentSource.index)
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
        id: replaceNextMenuP
        mytext :  "Next Version"
        shortcut : "Alt+N"
        onTriggered: shotgun_menu_action("substitute_with", "Next Version")
    }
    XsMenuItem {
        visible: false
        id: replacePreviousMenuP
        mytext :  "Previous Version"
        shortcut : "Alt+P"
        onTriggered: shotgun_menu_action("substitute_with", "Previous Version")
    }
    XsMenuItem {
        visible: false
        id: replaceLatestMenuP
        mytext :  "Latest Version"
        shortcut : "Alt+l"
        onTriggered: shotgun_menu_action("substitute_with", "Latest Version")
    }

    XsMenuItem {
        visible: false
        id: allVersionsMenuP
        mytext : "Shot Browser Shot..."
        // mytext : menuLiveLatest + "..."
        onTriggered: shotgun_menu_action("live_latest_versions", "")
    }
    XsMenuItem {
        visible: false
        id: relatedMenuP
        mytext : "Shot Browser Version Stream..."
        // mytext : menuLiveHistory + "..."
        onTriggered: shotgun_menu_action("live_version_history", "")
    }
    XsMenuItem {
        visible: false
        id: showNoteseMenuP
        mytext : "Shot Browser Notes..."
        // mytext : menuLiveNotes + "..."
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
            sessionWidget.sessionMenu.panelMenu.insertItem(5, referenceMenu)

            let m_index = 12
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, showNoteseMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, allVersionsMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, relatedMenu)

            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, replaceLatestMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, replacePreviousMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, replaceNextMenu)

            sessionWidget.sessionMenu.mediaMenu.insertMenu(m_index, substituteMenu)
            sessionWidget.sessionMenu.mediaMenu.insertMenu(m_index, compareMenu)

            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, downloadMissingMovieMenu)
            sessionWidget.sessionMenu.mediaMenu.insertItem(m_index, menuSep)

            sessionWidget.sessionMenu.mediaMenu.advancedMenu.insertItem(5, downloadMovieMenu)
            sessionWidget.sessionMenu.mediaMenu.revealMenu.insertItem(1, revealInIvyMenu)
            sessionWidget.sessionMenu.mediaMenu.revealMenu.insertItem(1, revealInShotgunMenu)

            sessionWidget.mediaMenu1.revealMenu.insertItem(1, revealInIvyMenuP)
            sessionWidget.mediaMenu1.revealMenu.insertItem(1, revealInShotgunMenuP)
            sessionWidget.mediaMenu1.advancedMenu.insertItem(5, downloadMovieMenuP)

            sessionWidget.mediaMenu1.insertItem(m_index, showNoteseMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, allVersionsMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, relatedMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, replaceLatestMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, replacePreviousMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, replaceNextMenuP)
            sessionWidget.mediaMenu1.insertMenu(m_index, substituteMenuP)
            sessionWidget.mediaMenu1.insertMenu(m_index, compareMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, downloadMissingMovieMenuP)
            sessionWidget.mediaMenu1.insertItem(m_index, menuSepP)

            toggleBrowser.visible = true
            showNoteseMenu.visible = true
            replaceLatestMenu.visible = true
            replacePreviousMenu.visible = true
            replaceNextMenu.visible = true
            allVersionsMenu.visible = true
            relatedMenu.visible = true
            menuSep.visible = true

            showNoteseMenuP.visible = true
            allVersionsMenuP.visible = true
            replaceLatestMenuP.visible = true
            replacePreviousMenuP.visible = true
            replaceNextMenuP.visible = true
            relatedMenuP.visible = true
            menuSepP.visible = true

            downloadMovieMenu.visible = true
            downloadMovieMenuP.visible = true

            downloadMissingMovieMenu.visible = true
            downloadMissingMovieMenuP.visible = true

            revealInIvyMenu.visible = true
            revealInIvyMenuP.visible = true

            revealInShotgunMenu.visible = true
            revealInShotgunMenuP.visible = true

            referenceMenu.visible = true
        }
    }

    Component.onDestruction: {
        sessionWidget.sessionMenu.panelMenu.removeItem(toggleBrowser)
        sessionWidget.sessionMenu.panelMenu.removeItem(referenceMenu)

        sessionWidget.sessionMenu.mediaMenu.removeItem(showNoteseMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(allVersionsMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(relatedMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(replaceLatestMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(replacePreviousMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(replaceNextMenu)
        sessionWidget.sessionMenu.mediaMenu.removeMenu(substituteMenu)
        sessionWidget.sessionMenu.mediaMenu.removeMenu(compareMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(downloadMissingMovieMenu)
        sessionWidget.sessionMenu.mediaMenu.removeItem(menuSep)

        sessionWidget.sessionMenu.mediaMenu.revealMenu.removeItem(revealInIvyMenu)
        sessionWidget.sessionMenu.mediaMenu.revealMenu.removeItem(revealInShotgunMenu)
        sessionWidget.sessionMenu.mediaMenu.advancedMenu.removeItem(downloadMovieMenu)

        sessionWidget.mediaMenu1.revealMenu.removeItem(revealInIvyMenuP)
        sessionWidget.mediaMenu1.revealMenu.removeItem(revealInShotgunMenuP)
        sessionWidget.mediaMenu1.advancedMenu.removeItem(downloadMovieMenuP)

        sessionWidget.mediaMenu1.removeItem(showNoteseMenuP)
        sessionWidget.mediaMenu1.removeItem(allVersionsMenuP)
        sessionWidget.mediaMenu1.removeItem(relatedMenuP)
        sessionWidget.mediaMenu1.removeItem(replaceLatestMenuP)
        sessionWidget.mediaMenu1.removeItem(replacePreviousMenuP)
        sessionWidget.mediaMenu1.removeItem(replaceNextMenuP)
        sessionWidget.mediaMenu1.removeMenu(substituteMenuP)
        sessionWidget.mediaMenu1.removeMenu(compareMenuP)
        sessionWidget.mediaMenu1.removeItem(downloadMissingMovieMenuP)
        sessionWidget.mediaMenu1.removeItem(menuSepP)
    }

    Connections {
        target: sessionModel
        function onBookmarkActorAddrChanged() {
            tag_timer.start()
        }
        function onRowsInserted(parent, first, last) {
            if(!sessionWidget.is_main_window)
                return

            if(parent == app_window.sessionModel.index(0, 0)) {
                connection_delay_timer.setTimeout(function(){
                    let playlists = app_window.sessionModel.search_list("Playlist", "typeRole", app_window.sessionModel.index(0, 0), 0, -1, 1)
                    playlists.forEach(
                        function (item, index) {
                            processPlaylist(item)
                        }
                    )
                }, 1000);
            }
        }
    }

    Connections {
        target: app_window.bookmarkModel
        function onBookmarkActorAddrChanged() {
            if(!sessionWidget.is_main_window || !app_window.bookmarkModel)
                return

            for(let i=0;i<app_window.bookmarkModel.length;i++){
                let ind = app_window.bookmarkModel.index(i, 0)
                if(ind.valid && app_window.bookmarkModel.getJSON(ind, "/metadata/shotgun")) {
                    addDecorator(app_window.bookmarkModel.get(ind, "uuidRole"))
                }
            }
        }
    }

    Connections {
        target: sessionWidget.playerWidget.viewport.playhead
        function onMediaUuidChanged(uuid) {
            if(browser.visible) {
                let mindex = app_window.sessionModel.search_recursive(uuid, "actorUuidRole", app_window.sessionModel.index(0, 0))
                if(mindex.valid) {
                    Future.promise(
                        mindex.model.getJSONFuture(mindex, "")
                    ).then(function(json_string) {
                        data_source.liveLinkMetadata = json_string
                    })
                }
            }
        }
    }

    Connections {
        target: app_window.mediaSelectionModel
        function onSelectionChanged(selected, deselected) {
            updateFromMediaSelection()
        }
    }

    Connections {
        target: browser
        function onVisibleChanged() {
            updateFromMediaSelection()
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

    // TreeTest {
    //     id: tree_test
    //     model: data_source.connected ? data_source.sequenceTreeModel(project_id_preference.value) : null
    // }

    ShotgunCreatePlaylist {
        id: create_dialog
        width: 350
        height: 250

        onAccepted: create_playlist_promise(
            data_source, playlist_uuid, project_id, playlist_name, create_dialog.site_name,create_dialog.playlist_type, error
        )
    }

    ShotgunPublishNotes {
        id: push_notes_dialog

        projectModel: data_source.termModels.projectModel
        groups: data_source.connected ? data_source.groups : null
        data_source: data_source
        playlists: app_window.playlistModel
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
        projectCurrentIndex: data_source.termModels.projectModel && data_source.termModels.projectModel.count ? data_source.termModels.projectModel.search(project_id_preference.value, "idRole") : -1
        projectModel: data_source.connected ? data_source.termModels.projectModel : null
        reviewLocationModel: data_source.connected ? data_source.termModels.reviewLocationModel : null
        siteModel: data_source.connected ? data_source.termModels.locationModel : null
        shotStatusModel: data_source.connected ? data_source.termModels.shotStatusModel : null
        referenceTagModel: data_source.connected ? data_source.termModels.referenceTagModel : null

        sequenceTreeModelFunc: data_source.connected ? data_source.sequenceTreeModel : null
        sequenceModelFunc: data_source.connected ? data_source.sequenceModel : null
        shotModelFunc: data_source.connected ? data_source.shotModel : null
        customEntity24ModelFunc: data_source.connected ? data_source.customEntity24Model : null
        shotSearchFilterModelFunc: data_source.connected ? data_source.shotSearchFilterModel : null
        playlistModelFunc: data_source.connected ? data_source.playlistModel : null

        shotResultsModel: data_source.resultModels.shotResultsModel
        shotTreeResultsModel: data_source.resultModels.shotTreeResultsModel
        playlistResultsModel: data_source.resultModels.playlistResultsModel
        editResultsModel: data_source.resultModels.editResultsModel
        referenceResultsModel: data_source.resultModels.referenceResultsModel
        noteResultsModel: data_source.resultModels.noteResultsModel
        noteTreeResultsModel: data_source.resultModels.noteTreeResultsModel
        mediaActionResultsModel: data_source.resultModels.mediaActionResultsModel

        shotPresetsModel: data_source.presetModels.shotPresetsModel
        shotTreePresetsModel: data_source.presetModels.shotTreePresetsModel
        playlistPresetsModel: data_source.presetModels.playlistPresetsModel
        editPresetsModel: data_source.presetModels.editPresetsModel
        referencePresetsModel: data_source.presetModels.referencePresetsModel
        notePresetsModel: data_source.presetModels.notePresetsModel
        noteTreePresetsModel: data_source.presetModels.noteTreePresetsModel
        mediaActionPresetsModel: data_source.presetModels.mediaActionPresetsModel

        shotFilterModel: data_source.presetModels.shotFilterModel
        playlistFilterModel: data_source.presetModels.playlistFilterModel
        editFilterModel: data_source.presetModels.editFilterModel
        referenceFilterModel: data_source.presetModels.referenceFilterModel
        noteFilterModel: data_source.presetModels.noteFilterModel
        mediaActionFilterModel: data_source.presetModels.mediaActionFilterModel

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


    function revealInIvy() {
        // can only process one..
        // so grab first entry from selection.
        if(app_window.mediaSelectionModel.selectedIndexes.length) {
            let mindex = app_window.mediaSelectionModel.selectedIndexes[0]
            Future.promise(
                mindex.model.getJSONFuture(mindex, "/metadata/shotgun/version/attributes/sg_ivy_dnuuid")
            ).then(function(json_string) {
                json_string = json_string.replace(/^"|"$/g, '')
                helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), helpers.getEnv("SHOT"), "--", "ivybrowser", json_string])
            })
        }
    }

    function revealPlaylistInShotgun(index=null) {
        if(index == null) {
            if(app_window.sourceSelectionModel.selectedIndexes.length) {
                index = app_window.sourceSelectionModel.selectedIndexes[0]
            }
        }

        Future.promise(
            index.model.getJSONFuture(index, "/metadata/shotgun/playlist/id")
        ).then(function(json_string) {
            json_string = json_string.replace(/^"|"$/g, '')
            helpers.openURL("http://shotgun/detail/Playlist/"+json_string)
        })
    }

    function revealInShotgun() {
        if(app_window.mediaSelectionModel.selectedIndexes.length) {
            let mindex = app_window.mediaSelectionModel.selectedIndexes[0]
            Future.promise(
                mindex.model.getJSONFuture(mindex, "/metadata/shotgun/version/id")
            ).then(function(json_string) {
                json_string = json_string.replace(/^"|"$/g, '')
                helpers.openURL("http://shotgun/detail/Version/"+json_string)
            })
        }
    }

    function updateFromMediaSelection() {
        if(browser.visible && app_window.mediaSelectionModel.selectedIndexes.length == 1) {
            let mindex = app_window.mediaSelectionModel.selectedIndexes[0]
            Future.promise(
                mindex.model.getJSONFuture(mindex, "")
            ).then(function(json_string) {
                data_source.liveLinkMetadata = json_string
            })
        }
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

    function renameTag(tag_id, oldname) {
        rename_ref_tag_dialog.tag_id = tag_id
        rename_ref_tag_dialog.text = oldname
        rename_ref_tag_dialog.open()
    }

    function removeTag(tag_id) {
        Future.promise(
            data_source.removeTagFuture(tag_id)
        ).then(function(json_string) {
        })
    }

    function tagSelectedMedia(tag_id) {
        let selected = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)

        if(selected.length) {
            for(let i = 0; i<selected.length; ++i ) {

                let mindex = selected[i]
                Future.promise(
                    mindex.model.getJSONFuture(mindex, "/metadata/shotgun/version/id")
                ).then(function(version_id) {
                    version_id = version_id.replace(/^"|"$/g, '')
                    // console.log("tagSelectedMedia", version_id, tag_id)
                    Future.promise(
                        data_source.tagEntityFuture("Version", version_id,  tag_id)
                    ).then(function(json_string) {
                        let new_tag = JSON.parse(json_string)
                        mindex.model.setJSONFuture(
                            mindex,
                            JSON.stringify(new_tag["data"]["relationships"]["tags"]),
                            "/metadata/shotgun/version/relationships/tags"
                        )
                        // console.log(data_source.getEntity("Version", version_id))
                        // console.log(json_string)
                    })
                })
            }
        }
    }

    function untagSelectedMedia(tag_id) {
        let selected = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)

        if(selected.length) {
            for(let i = 0; i<selected.length; ++i ) {

                let mindex = selected[i]
                Future.promise(
                    mindex.model.getJSONFuture(mindex, "/metadata/shotgun/version/id")
                ).then(function(version_id) {
                    version_id = version_id.replace(/^"|"$/g, '')
                    // console.log("tagSelectedMedia", version_id, tag_id)
                    Future.promise(
                        data_source.untagEntityFuture("Version", version_id,  tag_id)
                    ).then(function(json_string) {
                        let new_tag = JSON.parse(json_string)
                        mindex.model.setJSONFuture(
                            mindex,
                            JSON.stringify(new_tag["data"]["relationships"]["tags"]),
                            "/metadata/shotgun/version/relationships/tags"
                        )
                    })
                })
            }
        }
    }

    XsTimer {
        id: connection_delay_timer
    }

    function create_playlist(playlist_index=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ create_playlist(playlist_index); }, 1000);
            return
        }

        create_dialog.siteModel = data_source.termModels.locationModel
        create_dialog.siteCurrentIndex = data_source.termModels.locationModel.search(location_preference.value)
        create_dialog.projectModel = data_source.termModels.projectModel
        create_dialog.projectCurrentIndex = data_source.termModels.projectModel.search(project_id_preference.value, "idRole")
        create_dialog.playlistTypeModel = data_source.connected ? data_source.termModels.playlistTypeModel : null

        if(playlist_index && playlist_index.valid) {
            create_dialog.playlist_name = playlist_index.model.get(playlist_index, "nameRole")
            create_dialog.playlist_uuid = playlist_index.model.get(playlist_index, "actorUuidRole")
            get_valid_media_count(create_dialog, data_source, create_dialog.playlist_uuid, null)
            create_dialog.show()
        } else {
            let inds = app_window.sessionSelectionModel.selectedIndexes
            inds.forEach(
                function (item, index) {
                    let type = item.model.get(item, "typeRole")
                    if(type == "Playlist") {
                        // get playlist name
                        let uuid = item.model.get(item, "actorUuidRole")
                        let name = item.model.get(item, "nameRole")

                        create_dialog.playlist_name = name
                        create_dialog.playlist_uuid = uuid
                        get_valid_media_count(create_dialog, data_source, uuid, null)
                        create_dialog.show()
                    }
                }
            )
        }
    }

    function update_playlist(playlist_index=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ update_playlist(playlist); }, 1000);
            return
        }
        update_playlist_(control.data_source, playlist_index, control.error)
    }

    function refresh_playlist(playlist_index=null) {
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ refresh_playlist(playlist_index); }, 1000);
            return
        }

        if(playlist_index && playlist_index.valid) {
            playlist_index.model.set(playlist_index, true, "busyRole")
            let uuid = playlist_index.model.get(playlist_index, "actorUuidRole")
            let name = playlist_index.model.get(playlist_index, "nameRole")

            Future.promise(
                data_source.refreshPlaylistVersionsFuture(uuid)
            ).then(function(json_string) {
                playlist_index.model.set(playlist_index, false, "busyRole")
                ShotgunHelpers.handle_response(json_string, "Refresh Playlist", true, name, error)
            })
        } else {
            let inds = app_window.sessionSelectionModel.selectedIndexes
            inds.forEach(
                function (item, index) {
                    let type = item.model.get(item, "typeRole")
                    if(type == "Playlist") {
                        // get playlist name
                        let uuid = item.model.get(item, "actorUuidRole")
                        let name = item.model.get(item, "nameRole")

                        Future.promise(
                            data_source.refreshPlaylistVersionsFuture(uuid)
                        ).then(function(json_string) {
                            ShotgunHelpers.handle_response(json_string, "Refresh Playlist", true, name, error)
                        })
                    }
                }
            )
        }
    }

    function create_playlist_promise(data_source, playlist_uuid, project_id, name, location, playlist_type,  error) {
        Future.promise(
            data_source.createPlaylistFuture(playlist_uuid, project_id, name, location, playlist_type)
        ).then(function(json_string) {
            // load new playlist..
            try {
                var data = JSON.parse(json_string)
                if(data["data"]["id"]){
                    load_playlist(data_source, data["data"]["id"], data["data"]["attributes"]["code"])
                }
            } catch(err) {
                // ShotgunHelpers.handle_response(string, "ShotGrid Create Playlist", false, name, error)
            }
            ShotgunHelpers.handle_response(json_string, "ShotGrid Create Playlist", false, name, error)
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
        app_window.sessionModel.addTag(uuid, "Decorator", ui, uuid.toString()+"ShotgunDecorator");
    }

    function addMenusFull(uuid) {
        let ui = `
            import xStudio 1.0
            XsMenu {
                title: "ShotGrid Playlist"
                XsMenuItem {
                    mytext: qsTr("Create...")
                    onTriggered: sessionFunction.object_map["ShotgunRoot"].create_playlist(modelIndex())
                }

                XsMenuItem {
                    mytext: qsTr("Refresh")
                    onTriggered: sessionFunction.object_map["ShotgunRoot"].refresh_playlist(modelIndex())
                }

                XsMenuItem {
                    mytext: qsTr("Push Contents To ShotGrid...")
                    onTriggered: sessionFunction.object_map["ShotgunRoot"].update_playlist(modelIndex())
                }

                XsMenuItem {
                    mytext: qsTr("Reveal In ShotGrid...")
                    onTriggered: sessionFunction.object_map["ShotgunRoot"].revealPlaylistInShotgun(modelIndex())
                }
            }
        `
        app_window.sessionModel.addTag(uuid, "Menu", ui, uuid.toString()+"ShotGridMenu");
    }

    function addMenusPartial(uuid) {
        let ui = `
            import xStudio 1.0
            XsMenu {
                title: "ShotGrid Playlist"
                XsMenuItem {
                    mytext: qsTr("Create...")
                    onTriggered: sessionFunction.object_map["ShotgunRoot"].create_playlist(modelIndex())
                }
            }
        `
        app_window.sessionModel.addTag(uuid, "Menu", ui, uuid.toString()+"ShotGridMenu");
    }

    function processPlaylist(playlist_index) {
        if(playlist_index.valid) {
            let model = playlist_index.model
            let uuid = model.get(playlist_index, "actorUuidRole")

            // console.log("processPlaylist", model, uuid, model.getJSON(playlist_index, "/metadata/shotgun"))

            if(model.getJSON(playlist_index, "/metadata/shotgun")) {
                addDecorator(uuid)
                addMenusFull(uuid)
            } else {
                addMenusPartial(uuid)
            }
        }
    }

    function push_playlist_note_promise(data_source, payload, playlist_uuid, error) {
        Future.promise(
            data_source.pushPlaylistNotesFuture(payload, playlist_uuid)
        ).then(function(json_string) {
            ShotgunHelpers.handle_response(json_string, "ShotGrid Publish Note", false, "", error)
        })
    }

    function update_playlist_promise(data_source, playlist_uuid, name, error) {
        let index = app_window.sessionModel.search_recursive(playlist_uuid, "actorUuidRole")
        index.model.set(index, true, "busyRole")
        Future.promise(
            data_source.updatePlaylistVersionsFuture(playlist_uuid)
        ).then(function(json_string) {
            index.model.set(index, false, "busyRole")
            // load new playlist..
            ShotgunHelpers.handle_response(json_string, "Push Contents To ShotGrid ", false, name, error)
        })
    }

    function publish_notes(payload, playlist_uuid) {
        push_playlist_note_promise(data_source, payload, playlist_uuid, error)
    }

    function push_playlist_notes(selected_media, playlist_index=null) {
        push_playlist_notes_(selected_media, data_source, playlist_index, error)
    }

    function push_playlist_notes_(selected_media, data_source, playlist_index, error) {
        // notes should mirror site/show settings from playlist.
        if(!data_source.connected) {
            data_source.connected = true
            connection_delay_timer.setTimeout(function(){ push_playlist_notes_(selected_media, data_source, playlist_index, error); }, 1000);
            return
        }

        if(playlist_index && playlist_index.valid) {
            push_notes_dialog.publishSelected = selected_media
            push_notes_dialog.playlist_uuid = playlist_index.model.get(playlist_index, "actorUuidRole")
            push_notes_dialog.updatePublish()
            push_notes_dialog.show()
        } else {
            let inds = app_window.sessionSelectionModel.selectedIndexes
            inds.forEach(
                function (item, index) {
                    // find playlist..
                    let plind = app_window.sessionModel.getPlaylistIndex(item)
                    // get playlist name
                    let uuid = item.model.get(plind, "actorUuidRole")

                    push_notes_dialog.publishSelected = selected_media
                    push_notes_dialog.playlist_uuid = uuid
                    push_notes_dialog.updatePublish()
                    push_notes_dialog.show()
                }
            )
        }
    }

    function update_playlist_(data_source, playlist_index, error) {
        if(playlist_index && playlist_index.valid) {
            update_dialog.playlist_name = playlist_index.model.get(playlist_index, "nameRole")
            update_dialog.playlist_uuid = playlist_index.model.get(playlist_index, "actorUuidRole")
            get_valid_media_count(update_dialog, data_source, update_dialog.playlist_uuid, null)
            update_dialog.show()
        } else {
            let inds = app_window.sessionSelectionModel.selectedIndexes
            inds.forEach(
                function (item, index) {
                    let type = item.model.get(item, "typeRole")
                    if(type == "Playlist") {
                        // get playlist name
                        let uuid = item.model.get(item, "actorUuidRole")
                        let name = item.model.get(item, "nameRole")

                        update_dialog.playlist_name = name
                        update_dialog.playlist_uuid = uuid
                        get_valid_media_count(create_dialog, data_source, uuid, null)
                        update_dialog.show()
                    }
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

        if(key == "publish_playlist_notes") {
            push_playlist_notes(false)
        } else if(key == "publish_selected_notes") {
            push_playlist_notes(true)
        } else if(key == "notes_history") {
            show_browser()
            currentCategory = "Notes Tree"
            createPresetType(menuLiveNotes)
        } else if(key == "live_version_history") {
            show_browser()
            currentCategory = "Versions"
            createPresetType(menuLiveHistory)
        } else if(key == "live_latest_versions") {
            show_browser()
            currentCategory = "Versions"
            createPresetType(menuLiveLatest)
        } else if(key == "all_versions") {
            show_browser()
            currentCategory = "Versions"
            createPresetType("All")
        } else if(key == "substitute_with") {
            substitute_with(value)
        } else if(key == "compare_with") {
            compare_with(value)
        } else if(key == "shotgun_browser") {
            toggle_browser()
        } else if(key == "download_movie") {
            // download movies for media..
            for(let i = 0; i < value.length; ++i)
                download_movie(value[i])
        } else if(key == "download_missing_movie") {
            // for value playlist download all media that have missing current image.
            let model = value.model
            let cindex = model.index(0,0,value)
            let ccount = model.rowCount(cindex)
            for(let i =0; i< ccount; ++i) {
                // is online.
                let mindex = model.index(i,0,cindex)

                if(model.get(mindex, "mediaStatusRole") != "Online")
                    download_movie(mindex)
            }
        }
    }

    function download_movie(mindex) {
        Future.promise(data_source.addDownloadToMediaFuture(mindex.model.get(mindex, "actorUuidRole"))).then(
            function(result) {
                let jsn = JSON.parse(result)
                if(jsn.actor_uuid)
                    mindex.model.set(mindex, jsn.actor_uuid, "imageActorUuidRole")

            },
            function() {
            }
        )
    }


    function substitute_with(preset_name) {
        //for each selected media..
        let selected = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)

        if(!selected.length)
            return;

        let model = selected[0].model
        let items = []
        let media_parent = selected[0].parent

        for(let i = 0; i<selected.length; ++i ) {
            let parent_uuid = model.get(selected[i].parent.parent, "actorUuidRole")
            let next_media_uuid = model.get(selected[i].model.index(selected[i].row + 1, 0, selected[i].parent), "actorUuidRole")
            let json = model.getJSON(selected[i], "")
            let media_id = model.get(selected[i],"idRole")

            if(next_media_uuid == undefined)
                next_media_uuid = helpers.QVariantFromUuidString("")

            items.push([parent_uuid, next_media_uuid, json, media_id])
        }

        items.forEach(
            function (item, index) {
                browser.executeMediaActionQuery(preset_name, item[2],
                    function (data) {
                        // load item after existing
                        // delete existing..
                        let parent_uuid = item[0]
                        let next_media_uuid = item[1]

                        let result = JSON.parse(
                                data_source.addVersionToPlaylist(
                                JSON.stringify(data),
                                parent_uuid,
                                next_media_uuid
                            )
                        )
                        // select new item ?
                        if(result.length) {

                            connection_delay_timer.setTimeout(function(){
                                // delete
                                let remove_index = model.search(item[3], "idRole", media_parent)
                                model.removeRows(remove_index.row, 1, remove_index.parent)

                                let new_media = []
                                let tmp = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                                for(let i=0; i < tmp.length; ++i)
                                    if(tmp[i] != remove_index)
                                        new_media.push(tmp[i])
                                // remove deleted..

                                for(let i=0;i<result.length; i++) {
                                    let mind = model.search(helpers.QVariantFromUuidString(result[i]), "actorUuidRole", media_parent)
                                    new_media.push(mind)
                                }
                                app_window.mediaSelectionModel.select(
                                    helpers.createItemSelection(new_media.sort((a,b) => a.row - b.row )), ItemSelectionModel.ClearAndSelect
                                )

                            }, 500);
                        } else {
                            status_bar.normalMessage("No results", "Shot Browser - Substitute with")
                        }
                    }
                )
            }
        )
    }

    function compare_with(preset_name) {
        let selected = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)

        selected.forEach(
            function (item, index) {
                if(item.valid) {
                    browser.executeMediaActionQuery(preset_name, item.model.getJSON(item, ""),
                        function (data) {
                            // load item after existing
                            let parent_uuid = item.model.get(item.parent.parent, "actorUuidRole")
                            let media_uuid = item.model.get(item, "actorUuidRole")
                            let next_media_uuid = item.model.get(item.model.index(item.row + 1, 0, item.parent), "actorUuidRole")

                            if(next_media_uuid == undefined)
                                next_media_uuid = helpers.QVariantFromUuidString("")

                            let result = JSON.parse(
                                    data_source.addVersionToPlaylist(
                                    JSON.stringify(data),
                                    parent_uuid,
                                    next_media_uuid
                                )
                            )

                            // order matters,,

                            if(result.length) {
                                connection_delay_timer.setTimeout(function(){
                                    let indexs = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)
                                    for(let i=0;i<result.length; i++) {
                                        let new_index = item.model.search_recursive(helpers.QVariantFromUuidString(result[i]), "actorUuidRole", item.parent)
                                        indexs.push(new_index)
                                    }
                                    // order..

                                    app_window.mediaSelectionModel.select(
                                        helpers.createItemSelection(indexs.sort((a,b) => a.row - b.row )),
                                        ItemSelectionModel.ClearAndSelect
                                    )
                                }, 500);

                            } else {
                                status_bar.normalMessage("No results", "Shot Browser - Compare with")
                            }
                        }
                    )
                }
            }
        )
        playhead_attrs.compare = "A/B"
    }


// 2023-04-21 15:36:32.576] [xstudio] [info] processChildren PlayheadSelection 1
// "Could not convert argument 2 at"
//      "@file:///user_data/RND/dev/xstudio/build_rel/bin/plugin/qml/Shotgun.1/ShotgunRoot.qml:1047"
//      "@file:///user_data/RND/dev/xstudio/build_rel/bin/plugin/qml/Shotgun.1/ShotgunBrowserDialog.qml:227"
//      "@qrc:/extern/QuickPromise/promise.js:104"
//      "@qrc:/extern/QuickPromise/promise.js:284"
//      "@qrc:/extern/QuickPromise/promise.js:301"
//      "@qrc:/extern/QuickPromise/promise.js:248"
//      "@qrc:/extern/QuickPromise/promise.js:237"
//      "@qrc:/extern/QuickPromise/promise.js:28"
// "Passing incompatible arguments to C++ functions from JavaScript is dangerous and deprecated."
// "This will throw a JavaScript TypeError in future releases of Qt!"
// [2023-04-21 15:36:39.925] [xstudio] [warning] SEND
// qml: QModelIndex()
// qml: TypeError: Property 'clear' of object QModelIndex(5,0,0x9bad210,xstudio::ui::qml::SessionModel(0x343b2d0)) is not a function




    function createPresetType(mode) {
        browser.createPresetType(mode)
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
                load_playlist(data_source, item.id, item.name, error, index==0, preferred_visual, preferred_audio, flag_text, flag_colour)
            }
        )
    }

    // items are version metadata from shotgun.
    function addVersionsToPlaylist(uuid, items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        let query = ""

        if(typeof(items) == "string") {
            try {
                let j = JSON.parse(items)
                j["context"] = new Map()
                j["context"]["visual_source"] = preferred_visual
                j["context"]["audio_source"] = preferred_audio
                j["context"]["flag_text"] = flag_text
                j["context"]["flag_colour"] = flag_colour
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
                '"context": {' +
                '"visual_source": "' + preferred_visual + '",' +
                '"audio_source": "' + preferred_audio + '",' +
                '"flag_text": "' + flag_text + '",' +
                '"flag_colour": "' + flag_colour + '"' +
                '}'+
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
                        let index =  sessionSelectionModel.model.search_recursive(uuid,"actorUuidRole")
                        app_window.sessionFunction.setActivePlaylist(index)
                        app_window.requestActivate()
                        app_window.raise()
                        sessionWidget.playerWidget.forceActiveFocus()

                        // add media uuid to selection and focus it.
                        let mind = sessionSelectionModel.model.search("{"+data[0]+"}", "actorUuidRole", sessionSelectionModel.model.index(0,0, index))
                        if(mind.valid)
                            app_window.sessionFunction.setActiveMedia(mind)

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
                j["context"] = new Map()
                j["context"]["visual_source"] = preferred_visual
                j["context"]["audio_source"] = preferred_audio
                j["context"]["flag_text"] = flag_text
                j["context"]["flag_colour"] = flag_colour
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
                '"context": {' +
                '"visual_source": "' + preferred_visual + '",' +
                '"audio_source": "' + preferred_audio + '",' +
                '"flag_text": "' + flag_text + '",' +
                '"flag_colour": "' + flag_colour + '"' +
                '}'+
                '}'
            }
        }

        if(query.length) {
            var uuid

            if(!app_window.sessionSelectionModel.currentIndex.valid) {
                let index = app_window.sessionFunction.createPlaylist(name)
                uuid = index.model.get(index, "actorUuidRole")

                app_window.sessionFunction.setActivePlaylist(index)

                // app_window.sessionSelectionModel.setCurrentIndex(index, ItemSelectionModel.setCurrentIndex|ItemSelectionModel.ClearAndSelect)
            } else {
                uuid = app_window.sessionSelectionModel.currentIndex.model.get(app_window.sessionSelectionModel.currentIndex, "actorUuidRole")
            }

            let next_media_uuid = undefined
            let selected = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes)

            if(selected.length) {
                let mind = selected[selected.length-1]
                let nmind = mind.model.index(mind.row+1, 0, mind.parent)
                if(nmind.valid) {
                    next_media_uuid = nmind.model.get(nmind, "actorUuidRole")
                }
            }

            if(next_media_uuid == undefined)
                next_media_uuid = helpers.QVariantFromUuidString("")

            // add versions to playlist.

            Future.promise(
                data_source.addVersionToPlaylistFuture(
                    query,
                    uuid,
                    next_media_uuid
                )
            ).then(
                function(json_string) {
                    // select media uuids..
                    try {
                        var data = JSON.parse(json_string)
                        // should be array of uuids..

                        // let selection = app_window.mediaSelectionModel.selectedIndexes
                        let indexs = []

                        for(let i = 0; i< data.length; ++i) {
                            indexs.push(app_window.mediaSelectionModel.model.search_recursive(helpers.QVariantFromUuidString(data[i]), "actorUuidRole"))
                        }

                        app_window.mediaSelectionModel.select(helpers.createItemSelection(indexs), ItemSelectionModel.Select)

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
        let index = app_window.sessionFunction.createPlaylist(name)
        let uuid = index.model.get(index, "actorUuidRole")

        app_window.sessionFunction.setActivePlaylist(index)

        // app_window.sessionSelectionModel.setCurrentIndex(index, ItemSelectionModel.setCurrentIndex|ItemSelectionModel.ClearAndSelect)

        addVersionsToPlaylist(uuid, items, preferred_visual, preferred_audio, flag_text, flag_colour )
    }

    function addShotsToPlaylist(items, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        let index = app_window.currentSource.index

        if(index == undefined || !index.valid) {
            index = app_window.sessionFunction.createPlaylist("Shot Browser Media")
        }

        let uuid = index.model.get(index, "actorUuidRole")

        app_window.sessionFunction.setActivePlaylist(index)

        // app_window.sessionSelectionModel.setCurrentIndex(index, ItemSelectionModel.setCurrentIndex|ItemSelectionModel.ClearAndSelect)

        addVersionsToPlaylist(uuid, items, preferred_visual, preferred_audio, flag_text, flag_colour)
    }

    function load_playlist(data_source, id, name, error, make_active=true, preferred_visual="SG Movie", preferred_audio="SG Movie", flag_text="", flag_colour="") {
        // create new playlist
        let index = app_window.sessionFunction.createPlaylist(name)

        if(make_active){
            app_window.sessionFunction.setActivePlaylist(index)
        }

        let uuid = index.model.get(index, "actorUuidRole")
        index.model.set(index, true, "busyRole")

        // get playlist json from shotgun
        Future.promise(data_source.getPlaylistVersionsFuture(id)).then(function(json_string) {
            try {
                var data = JSON.parse(json_string)
                if(data["data"]){
                    Future.promise(index.model.setJSONFuture(index, JSON.stringify(data['data']), "/metadata/shotgun/playlist")).then(
                        function(result) {
                            addDecorator(uuid)
                            addMenusFull(uuid)
                        },
                        function() {
                        }
                    )

                    data["context"] = new Map()
                    data["context"]["visual_source"] = preferred_visual
                    data["context"]["audio_source"] = preferred_audio
                    data["context"]["flag_text"] = flag_text
                    data["context"]["flag_colour"] = flag_colour

                    // add versions to playlist.
                    Future.promise(data_source.addVersionToPlaylistFuture(JSON.stringify(data), uuid)).then(
                        function(json_string) {
                            if(make_active) {
                                var data = JSON.parse(json_string)
                                let index =  sessionSelectionModel.model.search_recursive(uuid,"actorUuidRole")

                                // add media uuid to selection and focus it.
                                let mind = sessionSelectionModel.model.search("{"+data[0]+"}", "actorUuidRole", sessionSelectionModel.model.index(0,0, index))
                                if(mind.valid)
                                    app_window.sessionFunction.setActiveMedia(mind)
                            }

                            index.model.set(index, false, "busyRole")
                            ShotgunHelpers.handle_response(json_string)
                        },
                        function() {
                            index.model.set(index, false, "busyRole")
                        }
                    )
                } else {
                    index.model.set(index, false, "busyRole")
                    error.title = "Load ShotGrid Playlist " + name
                    error.text = json_string
                    error.open()
                }
            }
            catch(err) {
                index.model.set(index, false, "busyRole")
                error.title = "Load ShotGrid Playlist " + name
                error.text = err + "\n" + json_string
                error.open()
                console.log(err, json_string)
            }
        })
    }
}
