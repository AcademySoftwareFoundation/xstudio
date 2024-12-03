// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import QuickFuture 1.0

import "../functions/"

XsPopupMenu {
    id: btnMenu
    visible: false
    menu_model_name: "media_list_menu_"

    property var panelContext: helpers.contextPanel(btnMenu)

    /**************************************************************

    Static Menu Items (most items in this menu are added dymanically
        from the backend - e.g. PlayheadActor, Viewport classes do this)

    ****************************************************************/

    Clipboard {
      id: clipboard
    }

    XsPreference {
        id: sessionLinkPrefix
        path: "/core/session/session_link_prefix"
    }

    Component.onCompleted: {
        // make sure the 'Add' sub-menu appears in the correct place
        helpers.setMenuPathPosition("Select", "media_list_menu_", 3)
        helpers.setMenuPathPosition("Add Media To", "media_list_menu_", 1)
        helpers.setMenuPathPosition("Copy To Clipboard", "media_list_menu_", 10)
        helpers.setMenuPathPosition("Reveal Source", "media_list_menu_", 15)
        helpers.setMenuPathPosition("Advanced", "media_list_menu_", 30)
        //helpers.setMenuPathPosition("Set Media Source", "media_list_menu_", 15.5)

        // need to reorder snippet menus..
        let rc = embeddedPython.mediaMenuModel.rowCount();
        for(let i=0; i < embeddedPython.mediaMenuModel.rowCount(); i++) {
            let fi = embeddedPython.mediaMenuModel.index(i, 0)
            let si = embeddedPython.mediaMenuModel.mapToSource(fi)
            let mp = si.model.get(si, "menuPathRole")
            helpers.setMenuPathPosition(mp,"media_list_menu_", 17 + ((1.0/rc)*i) )
        }
    }

    XsMenuModelItem {
        text: "New Playlist..."
        menuPath: "Add Media To"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                function(name, button) {
                    if(button == "Move Media") {
                        let pl = theSessionData.createPlaylist(name)
                        theSessionData.moveRows(mediaSelectionModel.selectedIndexes, 0, pl)
                    } else if(button == "Copy Media") {
                        let pl = theSessionData.createPlaylist(name)
                        theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, pl)
                    }
                },
                "Add To New Playlist",
                "Enter New Playlist Name",
                "Untitled Playlist",
                ["Cancel", "Move Media", "Copy Media"])
        }

        panelContext: btnMenu.panelContext
    }


    XsMenuModelItem {
        text: "New Subset..."
        menuPath: "Add Media To"
        menuItemPosition: 2
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                function(name, button) {
                    if(button == "Add Media") {
                        addToNewSubset(name)
                    }
                },
                "Add To New Subset",
                "Enter New Subset Name",
                "Untitled Subset",
                ["Cancel", "Add Media"])
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "New Contact Sheet..."
        menuPath: "Add Media To"
        menuItemPosition: 3
        menuModelName: btnMenu.menu_model_name
        panelContext: btnMenu.panelContext
        onActivated: {
            dialogHelpers.textInputDialog(
                function(name, button) {
                    if(button == "Add Media") {
                        addToNewContactSheet(name)
                    }
                },
                "Add To New Contact Sheet",
                "Enter New Contact Sheet Name",
                "Untitled Contact Sheet",
                ["Cancel", "Add Media"])
        }
    }

    XsMenuModelItem {
        text: "New Sequence..."
        menuPath: "Add Media To"
        menuItemPosition: 4
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                function(name, button) {
                    if(button == "Add Media") {
                        addToNewSequence(name)
                    }
                },
                "Add To New Sequence",
                "Enter New Sequence Name",
                "Untitled Sequence",
                ["Cancel", "Add Media"])
        }
        panelContext: btnMenu.panelContext
    }

    XsFlagMenuInserter {
        text: qsTr("Set Media Colour")
        panelContext: btnMenu.panelContext
        menuModelName: btnMenu.menu_model_name
        menuPath: ""
        menuPosition: 2
        onFlagSet: {
            let sindexs = mediaSelectionModel.selectedIndexes
            for(let i = 0; i< sindexs.length; i++) {
                theSessionData.set(sindexs[i], flag, "flagColourRole")

                if (flag_text)
                    theSessionData.set(sindexs[i], flag_text, "flagTextRole")
            }
        }
    }

    XsMenuModelItem {
        text: "All Media"
        menuItemType: "button"
        menuPath: "Select"
        menuItemPosition: 3
        menuModelName: btnMenu.menu_model_name
        hotkeyUuid: select_all_hotkey.uuid
        onActivated: selectAll()
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "All Offline Media"
        menuItemType: "button"
        menuPath: "Select"
        menuItemPosition: 3.2
        menuModelName: btnMenu.menu_model_name
        onActivated: selectAllOffline()
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Adjusted Media"
        menuItemType: "button"
        menuPath: "Select"
        menuItemPosition: 3.3
        menuModelName: btnMenu.menu_model_name
        onActivated: selectAllAdjusted()
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Invert Selection"
        menuItemType: "button"
        menuPath: "Select"
        menuItemPosition: 3.4
        menuModelName: btnMenu.menu_model_name
        onActivated: invertSelection()
        panelContext: btnMenu.panelContext
    }

    XsFlagMenuInserter {
        text: qsTr("Flagged Media")
        panelContext: btnMenu.panelContext
        menuModelName: btnMenu.menu_model_name
        menuPath: "Select"
        menuPosition: 3.5
        onFlagSet: selectFlagged(flag)
    }


    // XsMenuModelItem {
    //     text: "TEST"
    //     menuPath: ""
    //     menuItemPosition: 2.5
    //     menuModelName: btnMenu.menu_model_name
    //     onActivated: {
    //         let mlf = 14
    //         let c = theSessionData.getTimelineVisibleClipIndexes(currentMediaContainerIndex, mediaSelectionModel.selectedIndexes[0], mlf)
    //         if(c.length)
    //             console.log(theSessionData.getTimelineFrameFromClip(c[0], mlf))
    //     }
    //     panelContext: btnMenu.panelContext
    // }

    // XsMenuModelItem {
    //     text: "De-Select All"
    //     menuPath: ""
    //     menuItemPosition: 2
    //     menuModelName: btnMenu.menu_model_name
    //     hotkeyUuid: deselect_all_hotkey.uuid
    //     onActivated: {
    //         mediaList.deselectAll()
    //     }
    //     panelContext: btnMenu.panelContext
    // }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 5
        menuPath: ""
        menuModelName: btnMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Duplicate"
        menuPath: ""
        menuItemPosition: 6
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            let items = []

            for(let i=0;i<mediaSelectionModel.selectedIndexes.length;++i)
                items.push(helpers.makePersistent(mediaSelectionModel.selectedIndexes[i]))

            items.forEach(
                (item) => {
                    item.model.duplicateRows(item.row, 1, item.parent)
                }
            )
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Quick-View Selected"
        menuPath: ""
        menuItemPosition: 6.1
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            if (mediaSelectionModel.selectedIndexes.length > 8) {
                dialogHelpers.errorDialogFunc(
                    "Warning",
                    "The quick view feature is limited to a selection of 8 or fewer media items!"
                    )
                return;
            }
            for (var i in mediaSelectionModel.selectedIndexes) {
                let idx = mediaSelectionModel.selectedIndexes[i]
                var actor = theSessionData.get(idx, "actorRole")
                quick_view_launcher.launchQuickViewer([actor], "Off", -1, -1)
            }

        }
        panelContext: btnMenu.panelContext
    }


    XsMenuModelItem {
        menuPath: "Copy To Clipboard"
        text: "Selected File Names"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            let result = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")
            for(let i =0;i<result.length;i++) {
                result[i] = helpers.pathFromURL(result[i])
                result[i] = result[i].substr(result[i].lastIndexOf("/")+1)
            }
            clipboard.text = result.join("\n")
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        menuPath: "Copy To Clipboard"
        text: "Selected File Paths"
        menuItemPosition: 2
        menuModelName: btnMenu.menu_model_name

        onActivated: {
            let result = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")
            for(let i =0;i<result.length;i++) {
                result[i] = helpers.pathFromURL(result[i])
            }

            clipboard.text = result.join("\n")
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        menuPath: "Copy To Clipboard"
        text: "QuickView Shell Command"
        menuItemPosition: 3
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            let filenames = mediaSelectionModel.getSelectedMediaUrl("pathShakeRole")

            for(let i =0;i<filenames.length;i++) {
                filenames[i] = helpers.pathFromURL(filenames[i])
            }

            clipboard.text = "xstudio -l '" + filenames.join("' '") +"'"
        }

        panelContext: btnMenu.panelContext

    }

    XsMenuModelItem {
        menuPath: "Copy To Clipboard"
        text: "Email Link"
        menuItemPosition: 4
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            let name = encodeURIComponent(inspectedMediaSetProperties.values.nameRole)
            let prefix = "&" + name + "_media="
            let filenames = mediaSelectionModel.getSelectedMediaUrl()
            for(let i =0;i<filenames.length;i++) {
                filenames[i] = helpers.pathFromURL(filenames[i])
            }

            clipboard.text = sessionLinkPrefix.value + "xstudio://add_media?compare="+
                encodeURIComponent(currentPlayhead.compareMode)+"&playlist=" +
                name + prefix +
                filenames.join(prefix)
        }

        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "On Disk..."
        menuPath: "Reveal Source"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        onActivated: helpers.showURIS(mediaSelectionModel.getSelectedMediaUrl())
        panelContext: btnMenu.panelContext
    }

    /*XsMenuModelItem {
        text: "Foo"
        menuPath: "Set Media Source"
        menuItemType: "custom"
        choices: ["Moview", "EXR", "Review Proxy 1"]
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            console.log("Foo")
        }
        panelContext: btnMenu.panelContext
        customMenuQml: "import xStudio 1.0; XsMediaListSwitchSourceMenu {}"

    }*/
    XsMenuModelItem {
        text: "Snippet"
        menuItemType: "divider"
        menuItemPosition: 16
        menuPath: ""
        menuModelName: btnMenu.menu_model_name
    }

    Repeater {
        model: DelegateModel {
            model: embeddedPython.mediaMenuModel
            delegate: Item {XsMenuModelItem {
                text: nameRole
                menuPath: menuPathRole
                menuItemPosition: (index*0.01)+16
                menuModelName: btnMenu.menu_model_name
                onActivated: embeddedPython.pyEvalFile(scriptPathRole)
            }}
        }
    }


    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 29
        menuPath: ""
        menuModelName: btnMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Dump Metadata"
        menuPath: "Advanced"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            console.log(theSessionData.getJSON(mediaSelectionModel.selectedIndexes[0],""))
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Dump Source Metadata"
        menuPath: "Advanced"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            console.log(theSessionData.getJSON(mediaSelectionModel.getSourceIndex(mediaSelectionModel.selectedIndexes[0]),""))
        }
        panelContext: btnMenu.panelContext
    }


    XsMenuModelItem {
        text: "Dump JSON"
        menuPath: "Advanced"
        menuItemPosition: 2
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            console.log(theSessionData.get(mediaSelectionModel.selectedIndexes[0], "jsonTextRole"))
        }
        panelContext: btnMenu.panelContext
    }


    XsMenuModelItem {
        text: "Clear Cache"
        menuPath: "Advanced"
        menuItemPosition: 3
        menuModelName: btnMenu.menu_model_name
        onActivated: studio.clearImageCache()
        panelContext: btnMenu.panelContext

    }

    XsMenuModelItem {
        text: "Clear Selected Media From Cache"
        menuPath: "Advanced"
        menuItemPosition: 4
        menuModelName: btnMenu.menu_model_name
        onActivated: Future.promise(theSessionData.clearCacheFuture(mediaSelectionModel.selectedIndexes)).then(function(result){})
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Set Selected Media Rate"
        menuPath: "Advanced"
        menuItemPosition: 5
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            let mi = mediaSelectionModel.selectedIndexes[0]
            let ms = theSessionData.searchRecursive(theSessionData.get(mi, "imageActorUuidRole"), "actorUuidRole", mi)

            dialogHelpers.numberInputDialog(
                function(new_rate, button) {
                    if(button == "Set Rate") {
                        mediaSelectionModel.selectedIndexes.forEach(
                            (element) => {
                                let mi = theSessionData.searchRecursive(theSessionData.get(element, "imageActorUuidRole"), "actorUuidRole", element)
                                mi.model.set(mi, new_rate, "rateFPSRole")
                            }
                        )
                    }
                },
                "Set Media Rate",
                "Enter New Media Rate",
                 mi.model.get(ms, "rateFPSRole"),
                ["Cancel", "Set Rate"])

        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Load Additional Sources For Selected Media"
        menuPath: "Advanced"
        menuItemPosition: 6
        menuModelName: btnMenu.menu_model_name
        onActivated: theSessionData.gatherMediaFor(theSessionData.getPlaylistIndex(mediaSelectionModel.selectedIndexes[0]), mediaSelectionModel.selectedIndexes)
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Relink Selected Media"
        menuPath: "Advanced"
        menuItemPosition: 7
        menuModelName: btnMenu.menu_model_name
        onActivated: {
           dialogHelpers.showFileDialogFolderMode(
                function(path, folder, chaserFunc) {
                    theSessionData.relinkMedia(mediaSelectionModel.selectedIndexes, path)
                },
                file_functions.defaultSessionFolder(),
                "Relink media files")
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Decompose Selected Media"
        menuPath: "Advanced"
        menuItemPosition: 8
        menuModelName: btnMenu.menu_model_name
        onActivated: theSessionData.decomposeMedia(mediaSelectionModel.selectedIndexes)
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Reload Selected Media"
        menuPath: "Advanced"
        menuItemPosition: 9
        menuModelName: btnMenu.menu_model_name
        onActivated: theSessionData.rescanMedia(mediaSelectionModel.selectedIndexes)
        panelContext: btnMenu.panelContext
        hotkeyUuid: reload_selected_media_hotkey.uuid
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 99
        menuPath: ""
        menuModelName: btnMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Remove Selected"
        menuPath: ""
        menuItemPosition: 100
        menuModelName: btnMenu.menu_model_name
        hotkeyUuid: delete_selected.uuid
        onActivated: {
            deleteSelected()
        }
        panelContext: btnMenu.panelContext
    }
}
