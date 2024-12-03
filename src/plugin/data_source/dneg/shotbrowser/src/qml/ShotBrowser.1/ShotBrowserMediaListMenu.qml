// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

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

    XsHotkey {
        id: reload_playlist
        sequence: "Alt+r"
        name: "Reload Playlist"
        description: "Reload Playlist From ShotGrid Ordered"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(viewedMediaSetProperties.values.actorUuidRole), true
        )
    }

    XsMenuModelItem {
        text: "Pipeline"
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 25
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "In ShotGrid..."
        menuPath: "Reveal Source"
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
        text: "Publish Selected Media Notes"
        menuPath: ""
        menuItemPosition: 25.1
        menuModelName: "media_list_menu_"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromMedia(menuContext.mediaSelection)
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
        text: "Download SG Movie"
        menuPath: ""
        menuItemPosition: 26.2
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.downloadMovies(menuContext.mediaSelection)
    }

    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected|From London"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", "chn", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected|From London"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", "mtl", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected|From London"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", "mum", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected|From London"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", "syd", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected|From London"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("lon", "van", menuContext.mediaSelection)
    }


    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected|From Chennai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", "lon", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected|From Chennai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", "mtl", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected|From Chennai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", "mum", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected|From Chennai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", "syd", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected|From Chennai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("chn", "van", menuContext.mediaSelection)
    }



    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected|From Montreal"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl", "chn", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected|From Montreal"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl","lon", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected|From Montreal"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl","mum", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected|From Montreal"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl","syd", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected|From Montreal"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mtl","van", menuContext.mediaSelection)
    }


    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected|From Mumbai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum", "chn", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected|From Mumbai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum","lon", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected|From Mumbai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum","mtl", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected|From Mumbai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum","syd", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected|From Mumbai"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("mum","van", menuContext.mediaSelection)
    }




    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected|From Sydney"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd","chn", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected|From Sydney"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd","lon", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected|From Sydney"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd","mtl", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected|From Sydney"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd","mum", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected|From Sydney"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("syd","van", menuContext.mediaSelection)
    }


    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected|From Vancouver"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("van", "chn", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected|From Vancouver"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("van", "lon", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected|From Vancouver"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("van", "mtl", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected|From Vancouver"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("van", "mum", menuContext.mediaSelection)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected|From Vancouver"
        menuModelName: "media_list_menu_"
        onActivated: ShotBrowserHelpers.transferMedia("van", "syd", menuContext.mediaSelection)
    }


    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 7
        menuPath: "Transfer Selected"
        menuModelName: "media_list_menu_"
    }
    XsMenuModelItem {
        text: "Open Transfer Tool"
        menuItemPosition: 8
        menuPath: "Transfer Selected"
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

            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), helpers.getEnv("SHOT"), "--", "maketransfer"].concat(uuids))
        }

        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer Selected" sub menu in the top level menu
            setMenuPathPosition("Transfer Selected", 26.3)
        }
    }

    XsMenuModelItem {
        text: "Create SG Playlist..."
        menuPath: "Pipeline|Playlists"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_to_dialog.show()
            publish_to_dialog.playlistProperties = viewedMediaSetProperties
        }
    }

    XsMenuModelItem {
        text: "Reload SG Playlist"
        menuPath: "Pipeline|Playlists"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(viewedMediaSetProperties.values.actorUuidRole)
        )
    }

    XsMenuModelItem {
        text: "Reload SG Playlist (Ordered)"
        // enabled: false
        menuPath: "Pipeline|Playlists"
        menuItemPosition: 2.5
        menuModelName: "main menu bar"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(viewedMediaSetProperties.values.actorUuidRole),
            true
        )
    }

    XsMenuModelItem {
        text: "Push Media To SG Playlist"
        menuPath: "Pipeline|Playlists"
        menuItemPosition: 3
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            sync_to_dialog.show()
            sync_to_dialog.playlistProperties = viewedMediaSetProperties
        }
    }

    XsMenuModelItem {
        text: "Publish Playlist Notes"
        menuPath: "Pipeline|Notes"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromPlaylist(helpers.QVariantFromUuidString(viewedMediaSetProperties.values.actorUuidRole))
        }
    }

    XsMenuModelItem {
        text: "Publish Selected Media Notes"
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
        text: "Create SG Playlist..."
        menuPath: "Playlists"
        menuItemPosition: 1
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_to_dialog.show()
            publish_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
        Component.onCompleted: {
            setMenuPathPosition("Playlists", 10.1)
        }
    }

    XsMenuModelItem {
        text: "Reload SG Playlist"
        menuPath: "Playlists"
        menuItemPosition: 2
        menuModelName: "playlist_context_menu"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(inspectedMediaSetProperties.values.actorUuidRole)
        )
    }

    XsMenuModelItem {
        text: "Reload SG Playlist (Ordered)"
        // enabled: false
        menuPath: "Playlists"
        menuItemPosition: 2.5
        menuModelName: "playlist_context_menu"
        onActivated: ShotBrowserHelpers.syncPlaylistFromShotGrid(
            helpers.QUuidFromUuidString(viewedMediaSetProperties.values.actorUuidRole),
            true
        )
        hotkeyUuid: reload_playlist.uuid
    }


    XsMenuModelItem {
        text: "Push Media To SG Playlist"
        menuPath: "Playlists"
        menuItemPosition: 3
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            sync_to_dialog.show()
            sync_to_dialog.playlistProperties = inspectedMediaSetProperties
        }
    }

    XsMenuModelItem {
        text: "Reveal In Shotgrid"
        menuPath: "Playlists"
        menuItemPosition: 4
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            ShotBrowserHelpers.revealPlaylistInShotgrid(sessionSelectionModel.selectedIndexes)
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
        text: "Publish Playlist Notes"
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

    XsMenuModelItem {
        text: "Publish Selected Media Notes"
        menuPath: "Notes"
        menuItemPosition: 2
        menuModelName: "playlist_context_menu"
        onActivated: {
            ShotBrowserEngine.connected = true
            publish_notes.show()
            publish_notes.publishFromMedia(mediaSelectionModel.selectedIndexes)
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
}
