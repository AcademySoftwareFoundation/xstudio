// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0
import ShotBrowser 1.0
import xStudio 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import QuickFuture 1.0

Item {

    Clipboard {
      id: clipboard
    }

    // Note: For each instance of the ShotBrowser panel, we will have an
    // instance of THIS item. As such, the 'menu_model_name' needs to be
    // unique for each instance, so it has its own model data in the backend
    // from which the actual menu instance (of which there will also be
    // multiple instances) is built. See ShotBrowserPanel

    // Create a menu 'Some Menu' with an item in it that says 'Do Something'

    XsPreference {
       id: fullTransfer
       path: "/plugin/data_source/shotbrowser/transfer/full"
    }

    XsPreference {
       id: transferLeafs
       path: "/plugin/data_source/shotbrowser/transfer/leafs"
    }

    XsPreference {
        id: projectPref
        path: "/plugin/data_source/shotbrowser/browser/project"
    }


    property var leaves: fullTransfer.value ? [] : transferLeafs.value

    function getOffline() {
        var selection = []

        for (var i = 0; i < appWindow.mediaListModelData.rowCount(); ++i) {
            let si = appWindow.mediaListModelData.rowToSourceIndex(i)
            let state = theSessionData.get(si, "mediaStatusRole")
            if(state != undefined && state != "Online") {
                theSessionData.fetchMoreWait(si)
                selection.push(si)
            }
        }

        appWindow.mediaSelectionModel.select(
            helpers.createItemSelection(selection),
            ItemSelectionModel.ClearAndSelect
        )

        return selection
    }


    function getOfflineTimeline() {
        let rootIndex = theSessionData.index(2, 0, theSessionData.lastTimelineIndex)
        let tindex = theSessionData.getTimelineIndex(rootIndex)
        let mlist = theSessionData.index(0, 0, tindex)
        let clips = theSessionData.searchRecursiveList(
            "Clip", "typeRole", rootIndex,0,-1,-1
        )
        // with media uuid
        let clipsWithBadMedia = []
        let bad_media = []
        for(let i = 0; i< clips.length; i++) {
            let cmu = theSessionData.get(clips[i], "clipMediaUuidRole")
            if(cmu != undefined && cmu != "{00000000-0000-0000-0000-000000000000}") {
                // test media..
                // locate media index..
                let mindex = theSessionData.search(cmu, "actorUuidRole", mlist)
                if(mindex.valid) {
                    // check media offline
                    theSessionData.fetchMoreWait(mindex)
                    let state = theSessionData.get(mindex, "mediaStatusRole")
                    if(state != undefined && state != "Online") {
                        clipsWithBadMedia.push(clips[i])
                        bad_media.push(mindex)
                    }
                } else {
                    clipsWithBadMedia.push(clips[i])
                }
            }
        }

        theSessionData.makeTimelineSelection(tindex, clipsWithBadMedia)

        return bad_media
    }

    function getMediaFromClips(clips=[]) {
        let media = []
        let rootIndex = theSessionData.index(2, 0, theSessionData.lastTimelineIndex)
        let tindex = theSessionData.getTimelineIndex(rootIndex)
        let mlist = theSessionData.index(0, 0, tindex)

        for(let i = 0; i< clips.length; i++) {
            let cmu = theSessionData.get(clips[i], "clipMediaUuidRole")
            if(cmu != undefined && cmu != "{00000000-0000-0000-0000-000000000000}") {
                // test media..
                // locate media index..
                let mindex = theSessionData.search(cmu, "actorUuidRole", mlist)
                if(mindex.valid) {
                    media.push(mindex)
                }
            }
        }

        return media
    }

    XsHotkey {
        id: reload_playlist
        sequence: "Alt+r"
        name: "Reload Playlist"
        description: "Reload Playlist From ShotGrid Ordered"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole), true
        )
        componentName: "ShotBrowser"
    }

    XsMenuModelItem {
        text: "Pipeline"
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 200
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "In ShotGrid..." + (enabled ? "" : " (Production Only)")
        menuPath: "Reveal Source"
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuItemPosition: 2
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.revealMediaInShotgrid(menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "In Ivy..."
        menuPath: "Reveal Source"
        menuItemPosition: 3
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.revealMediaInIvy(menuContext.mediaSelection)
    }

    XsMenuModelItem {
        text: "Publish Media Notes..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Publish"
        menuItemPosition: 2
        menuModelName: "media_list_menu_"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromMedia(menuContext.mediaSelection)
        }
        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer" sub menu in the top level menu
            setMenuPathPosition("Publish", 210)
        }

    }

    // XsMenuModelItem {
    //     text: "Download Missing SG Previews"
    //     menuPath: ""
    //     menuItemPosition: 26.1
    //     menuModelName: "media_list_menu_"
    //     onActivated: ShotBrowserHelpers.downloadMissingMovies(menuContext.mediaSelection)
    // }

    XsMenuModelItem {
        text: "Refresh SG Metadata"
        menuPath: ""
        menuItemPosition: 260
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.refreshMetadata(menuContext.mediaSelection)
    }

    XsMenuModelItem {
        text: "Download SG Movie"
        menuPath: "Media Actions"
        menuItemPosition: 261
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.downloadMovies(menuContext.mediaSelection)
    }








    XsHotkey {
        id: qc_offline_current
        name: "Offline / Current"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline())
            } else {
                ShotBrowserHelpers.useCache(getOffline())
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_current
        name: "Selected / Current"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection))
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes)
            }
        }
        componentName: "Quick Cache"
    }


    XsHotkey {
        id: qc_offline_movie_dneg
        name: "Offline / movie_dneg"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline(), "movie_dneg")
            } else {
                ShotBrowserHelpers.useCache(getOffline(), "movie_dneg")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_movie_dneg
        name: "Selected / movie_dneg"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), "movie_dneg")
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes, "movie_dneg")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_offline_client_movie
        name: "Offline / client_movie"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline(), "client_movie")
            } else {
                ShotBrowserHelpers.useCache(getOffline(), "client_movie")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_client_movie
        name: "Selected / client_movie"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), "client_movie")
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes, "client_movie")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_offline_review_proxy_1
        name: "Offline / review_proxy_1"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline(), "review_proxy_1")
            } else {
                ShotBrowserHelpers.useCache(getOffline(), "review_proxy_1")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_review_proxy_1
        name: "Selected / review_proxy_1"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), "review_proxy_1")
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes, "review_proxy_1")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_offline_review_proxy_2
        name: "Offline / review_proxy_2"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline(), "review_proxy_2")
            } else {
                ShotBrowserHelpers.useCache(getOffline(), "review_proxy_2")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_review_proxy_2
        name: "Selected / review_proxy_2"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), "review_proxy_2")
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes, "review_proxy_2")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_offline_main_proxy0
        name: "Offline / main_proxy0"
        description: "Quick Cache Offline media/clips"
        onActivated: (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getOfflineTimeline(), "main_proxy0")
            } else {
                ShotBrowserHelpers.useCache(getOffline(), "main_proxy0")
            }
        }
        componentName: "Quick Cache"
    }

    XsHotkey {
        id: qc_selected_main_proxy0
        name: "Selected / main_proxy0"
        description: "Quick Cache Selected media/clip"
        onActivated:  (context) => {
            if(context.includes("timeline")) {
                ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), "main_proxy0")
            } else {
                ShotBrowserHelpers.useCache(mediaSelectionModel.selectedIndexes, "main_proxy0")
            }
        }
        componentName: "Quick Cache"
    }


    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 3.4
        menuPath: ""
        menuModelName: "timeline_clip_menu_"
    }

 XsMenuModelItem {
        text: "Current"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_current.uuid
        menuItemPosition: 1
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection))
    }

    XsMenuModelItem {
        text: "movie_dneg"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_movie_dneg.uuid
        menuItemPosition: 2
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), text)
    }

    XsMenuModelItem {
        text: "client_movie"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_client_movie.uuid
        menuItemPosition: 3
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), text)
    }

    XsMenuModelItem {
        text: "review_proxy_1"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_review_proxy_1.uuid
        menuItemPosition: 4
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), text)
    }

    XsMenuModelItem {
        text: "review_proxy_2"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_review_proxy_2.uuid
        menuItemPosition: 5
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), text)
    }

   XsMenuModelItem {
        text: "main_proxy0"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_main_proxy0.uuid
        menuItemPosition: 6
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getMediaFromClips(sessionData.currentTimelineSelection), text)
        Component.onCompleted: setMenuPathPosition("Quick Cache Selected", 3.6)
    }


    XsMenuModelItem {
        text: "Current"
        hotkeyUuid: qc_offline_current.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 1
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline())
    }

    XsMenuModelItem {
        text: "movie_dneg"
        hotkeyUuid: qc_offline_movie_dneg.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 2
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline(), text)
    }

    XsMenuModelItem {
        text: "client_movie"
        hotkeyUuid: qc_offline_client_movie.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 3
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline(), text)
    }

    XsMenuModelItem {
        text: "review_proxy_1"
        hotkeyUuid: qc_offline_review_proxy_1.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 4
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline(), text)
    }

    XsMenuModelItem {
        text: "review_proxy_2"
        hotkeyUuid: qc_offline_review_proxy_2.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 5
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline(), text)
    }

   XsMenuModelItem {
        text: "main_proxy0"
        hotkeyUuid: qc_offline_main_proxy0.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 6
        menuModelName: "timeline_clip_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOfflineTimeline(), text)
        Component.onCompleted: setMenuPathPosition("Quick Cache Offline", 3.5)
    }




    XsMenuModelItem {
        text: "Current"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_current.uuid
        menuItemPosition: 1
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection)
    }

    XsMenuModelItem {
        text: "movie_dneg"
        hotkeyUuid: qc_selected_movie_dneg.uuid
        menuPath: "Quick Cache Selected"
        menuItemPosition: 2
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection, text)
    }

    XsMenuModelItem {
        text: "client_movie"
        hotkeyUuid: qc_selected_client_movie.uuid
        menuPath: "Quick Cache Selected"
        menuItemPosition: 3
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection, text)
    }

    XsMenuModelItem {
        text: "review_proxy_1"
        hotkeyUuid: qc_selected_review_proxy_1.uuid
        menuPath: "Quick Cache Selected"
        menuItemPosition: 4
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection, text)
    }

    XsMenuModelItem {
        text: "review_proxy_2"
        hotkeyUuid: qc_selected_review_proxy_2.uuid
        menuPath: "Quick Cache Selected"
        menuItemPosition: 5
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection, text)
    }

   XsMenuModelItem {
        text: "main_proxy0"
        menuPath: "Quick Cache Selected"
        hotkeyUuid: qc_selected_main_proxy0.uuid
        menuItemPosition: 6
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(menuContext.mediaSelection, text)
        Component.onCompleted: setMenuPathPosition("Quick Cache Selected", 262)
    }

    XsMenuModelItem {
        text: "Current"
        hotkeyUuid: qc_offline_current.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 1
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline())
    }

    XsMenuModelItem {
        text: "movie_dneg"
        menuPath: "Quick Cache Offline"
        hotkeyUuid: qc_offline_movie_dneg.uuid
        menuItemPosition: 2
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline(), text)
    }

    XsMenuModelItem {
        text: "client_movie"
        hotkeyUuid: qc_offline_client_movie.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 3
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline(), text)
    }

    XsMenuModelItem {
        text: "review_proxy_1"
        hotkeyUuid: qc_offline_review_proxy_1.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 4
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline(), text)
    }

    XsMenuModelItem {
        text: "review_proxy_2"

        hotkeyUuid: qc_offline_review_proxy_2.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 5
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline(), text)
    }

   XsMenuModelItem {
        text: "main_proxy0"
        hotkeyUuid: qc_offline_main_proxy0.uuid
        menuPath: "Quick Cache Offline"
        menuItemPosition: 6
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.useCache(getOffline(), text)
        Component.onCompleted: setMenuPathPosition("Quick Cache Offline", 261)
    }








    XsMenuModelItem {
        text: "True"
        menuItemPosition: 1
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Set Status|Is Hero"+ (enabled ? "" : " (Production Only)")
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.markAsHero(menuContext.mediaSelection, true)
    }

    XsMenuModelItem {
        text: "False"
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuItemPosition: 2
        menuPath: "Set Status|Is Hero"+ (enabled ? "" : " (Production Only)")
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.markAsHero(menuContext.mediaSelection, false)
        Component.onCompleted: {
            setMenuPathPosition("Set Status|Is Hero"+ (enabled ? "" : " (Production Only)"), 3)
        }
    }

    XsMenuModelItem {
        text: "To Here"
        menuItemPosition: 1
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia(helpers.getEnv("DNSITEDATA_SHORT_NAME"), menuContext.mediaSelection, leaves)
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 1.5
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 2
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", menuContext.mediaSelection, leaves)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl", menuContext.mediaSelection, leaves)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum", menuContext.mediaSelection, leaves)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 4
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", menuContext.mediaSelection, leaves)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd", menuContext.mediaSelection, leaves)
    }
    // XsMenuModelItem {
    //     text: "To Vancouver"
    //     menuItemPosition: 6
    //     menuPath: "Transfer"
    //     menuModelName: "media_list_menu_"
    //     onActivated: ShotBrowserHelpers.transferMedia("van", menuContext.mediaSelection)
    // }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 7
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
    }
    XsMenuModelItem {
        text: "Open Transfer Tool"
        menuItemPosition: 8
        menuPath: "Transfer"
        menuModelName: "media_list_menu_"
        onActivated: {
            let uuids = []
            if(menuContext.mediaSelection.length) {
                // get stalk uuids..
                let m = menuContext.mediaSelection[0].model
                for(let i =0; i< menuContext.mediaSelection.length; i++) {
                    let meta = JSON.parse(theSessionData.getJSON(menuContext.mediaSelection[i], "/metadata/shotgun/version/attributes/sg_ivy_dnuuid"))
                    if(meta)
                        uuids.push(meta)
                }
            }

            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), "--", "maketransfer"].concat(uuids))
        }

        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer" sub menu in the top level menu
            setMenuPathPosition("Transfer", 220)
        }
    }

    XsMenuModelItem {
        text: "Create SG Playlist..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_to_dialog.show()
            publish_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
        Component.onCompleted: {
            helpers.setMenuPathPosition("Pipeline", "main menu bar", 3.0)
        }

    }

    XsMenuModelItem {
        text: "Create Reference Playlists"
        menuPath: "Pipeline|Reference"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            let m = ShotBrowserEngine.presetsModel.termModel("Project")
            ShotBrowserHelpers.createReferencePlaylists(
                m.get(
                    m.searchRecursive(projectPref.value, "nameRole"),
                    "idRole"
                )
            )
        }
    }


    XsMenuModelItem {
        text: "Add SG Playlist from Clipboard"
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: {
            let result = /.*\/Playlist\/(\d+).*/.exec(clipboard.text)
            ShotBrowserHelpers.loadShotGridPlaylist(result[1])
        }
    }

    XsMenuModelItem {
        text: "Reload SG Playlist"
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 2.1
        menuModelName: "main menu bar"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole)
        )
    }

    XsMenuModelItem {
        text: "Reload SG Playlist (Ordered)"
        // enabled: false
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 2.5
        menuModelName: "main menu bar"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole),
            true
        )
    }

    XsMenuModelItem {
        text: "Push Media To SG Playlist" + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 3
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            sync_to_dialog.show()
            sync_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
    }

    XsMenuModelItem {
        text: "Reveal In ShotGrid..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Pipeline|ShotGrid Playlists"
        menuItemPosition: 4
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            ShotBrowserHelpers.revealPlaylistInShotgrid(sessionSelectionModel.selectedIndexes)
        }
    }

    XsMenuModelItem {
        text: "Publish Playlist Notes" + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Pipeline|Notes"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromPlaylist(helpers.QVariantFromUuidString(inspectedMediaSetProperties.values.actorUuidRole))
        }
    }

    XsMenuModelItem {
        text: "Publish Selected Media Notes" + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Pipeline|Notes"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromMedia(mediaSelectionModel.selectedIndexes)
        }
    }


    XsMenuModelItem {
        menuItemType: "divider"
        text: "Pipeline"
        menuPath: ""
        menuItemPosition: 10
        menuModelName: "playlist_context_menu"
    }


    XsMenuModelItem {
        text: "Create SG Playlist..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 1
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_to_dialog.show()
            publish_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
        Component.onCompleted: {
            setMenuPathPosition("ShotGrid Playlists", 10.1)
        }
    }

    XsMenuModelItem {
        text: "Add SG Playlist from Clipboard"
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 2
        menuModelName: "playlist_context_menu"
        onActivated: {
            let result = /.*\/Playlist\/(\d+).*/.exec(clipboard.text)
            ShotBrowserHelpers.loadShotGridPlaylist(result[1])
        }
    }

    XsMenuModelItem {
        text: "Reload SG Playlist"
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 2
        menuModelName: "playlist_context_menu"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole)
        )
    }

    XsMenuModelItem {
        text: "Reload SG Playlist (Ordered)"
        // enabled: false
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 2.5
        menuModelName: "playlist_context_menu"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole),
            true
        )
        hotkeyUuid: reload_playlist.uuid
    }


    XsMenuModelItem {
        text: "Push Media To SG Playlist" + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 3
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            sync_to_dialog.show()
            sync_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
    }

    XsMenuModelItem {
        text: "Reveal In ShotGrid..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "ShotGrid Playlists"
        menuItemPosition: 4
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            ShotBrowserHelpers.revealPlaylistInShotgrid(sessionSelectionModel.selectedIndexes)
        }
    }

    XsMenuModelItem {
        text: "Create Reference Playlists"
        menuPath: "Reference"
        menuItemPosition: 1
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            let m = ShotBrowserEngine.presetsModel.termModel("Project")
            ShotBrowserHelpers.createReferencePlaylists(
                m.get(
                    m.searchRecursive(projectPref.value, "nameRole"),
                    "idRole"
                )
            )
        }
        Component.onCompleted: {
            setMenuPathPosition("Reference", 10.01)
        }
    }

    XsMenuModelItem {
        text: "SG Playlist Link "
        menuPath: "Copy To Clipboard"
        menuItemPosition: 4
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            clipboard.text = ShotBrowserHelpers.resolvePlaylistLink(sessionSelectionModel.selectedIndexes).join("\n")
        }
    }

    XsMenuModelItem {
        text: "Publish Playlist Notes" + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuPath: "Notes"
        menuItemPosition: 1
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromPlaylist(helpers.QVariantFromUuidString(inspectedMediaSetProperties.values.actorUuidRole))
        }
        Component.onCompleted: {
            setMenuPathPosition("Notes", 10.2)
        }
    }

    XsSBPublishNotesDialog {
        id: publish_notes
        property real btnHeight: XsStyleSheet.widgetStdHeight + 4
    }

    XsSBSyncPlaylistToShotGridDialog {
        id: sync_to_dialog
        width: 350
        height: 150
    }

    XsSBPublishPlaylistToShotGridDialog {
        id: publish_to_dialog
        width: 500
        height: 350
    }

    // This is required to create the application XsConformTool instance that
    // adds some default conform menus
    Component.onCompleted: {
        appWindow.createConformTool()
    }

}
