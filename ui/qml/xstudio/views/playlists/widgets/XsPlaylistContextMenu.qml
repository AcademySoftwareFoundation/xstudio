// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.clipboard 1.0
import "."

XsPopupMenu {

    id: contextMenu
    visible: false
    menu_model_name: "playlist_context_menu"

    Clipboard {
      id: clipboard
    }

    XsPlaylistPlusMenu {
        menu_model_name: "playlist_context_menu"
        path: "Add New"
    }

    // XsMenuModelItem {
    //     menuItemType: "divider"
    //     menuPath: ""
    //     menuItemPosition: -1.0
    //     menuModelName: contextMenu.menu_model_name
    // }

    Component.onCompleted: {
        // make sure the 'Add' sub-menu appears in the correct place
        helpers.setMenuPathPosition("Add", "playlist_context_menu", -2.0)
        helpers.setMenuPathPosition("Copy To Clipboard", "playlist_context_menu", 0.8)
        helpers.setMenuPathPosition("Cleanup", "playlist_context_menu", 22.0)
        helpers.setMenuPathPosition("Export", "playlist_context_menu", 4.0)
        // need to reorder snippet menus..
        let rc = embeddedPython.playlistMenuModel.rowCount();
        for(let i=0; i < embeddedPython.playlistMenuModel.rowCount(); i++) {
            let fi = embeddedPython.playlistMenuModel.index(i, 0)
            let si = embeddedPython.playlistMenuModel.mapToSource(fi)
            let mp = si.model.get(si, "menuPathRole")
            helpers.setMenuPathPosition(mp,"playlist_context_menu", 9 + ((1.0/rc)*i) )
        }
    }

    // property idenfies the 'panel' that is the anticedent of this
    // menu instance. As this menu is instanced multiple times in the
    // xstudio interface we use this context property to ensure our
    // 'onActivated' callback/signal is only triggered in the corresponding
    // XsMenuModelItem instance.
    property var panelContext: helpers.contextPanel(contextMenu)

    XsFlagMenuInserter {
        text: "Set Colour"
        panelContext: contextMenu.panelContext
        menuModelName: contextMenu.menu_model_name
        menuPath: ""
        menuPosition: 0.0
        onFlagSet: {
            for (var i = 0; i < sessionSelectionModel.selectedIndexes.length; ++i) {
                let index = sessionSelectionModel.selectedIndexes[i]
                theSessionData.set(index, flag, "flagColourRole")
                if (flag_text)
                    theSessionData.set(index, flag_text, "flagTextRole")
            }
        }
    }

   // XsMenuModelItem {
   //      text: qsTr("Dump JSON")
   //      menuPath: "Debug"
   //      menuItemPosition: 100
   //      menuModelName: contextMenu.menu_model_name
   //      onActivated: {
   //          for(let i=0;i<sessionSelectionModel.selectedIndexes.length;i++) {
   //              let idx = sessionSelectionModel.selectedIndexes[i]
   //              console.log(idx.model.get(idx, "jsonTextRole"))
   //              for(let j=0; j< theSessionData.rowCount(idx); j++) {
   //                  let idxx = theSessionData.index(j,0,idx)
   //                  console.log(idx.model.get(idxx, "jsonTextRole"))
   //                  for(let k=0; k< theSessionData.rowCount(idxx); k++) {
   //                      let idxxx = theSessionData.index(k,0,idxx)
   //                      console.log(idx.model.get(idxxx, "jsonTextRole"))
   //                  }
   //              }
   //          }
   //      }
   //      panelContext: contextMenu.panelContext
   //  }

   // XsMenuModelItem {
   //      text: qsTr("Dump Metadata")
   //      menuPath: "Debug"
   //      menuItemPosition: 100
   //      menuModelName: contextMenu.menu_model_name
   //      onActivated: {
   //          for(let i=0;i<sessionSelectionModel.selectedIndexes.length;i++) {
   //              let idx = sessionSelectionModel.selectedIndexes[i]
   //              console.log(theSessionData.getJSON(idx, ""))
   //          }
   //      }
   //      panelContext: contextMenu.panelContext
   //  }

    XsMenuModelItem {
        text: "Selected Names"
        menuItemType: "button"
        menuPath: "Copy To Clipboard"
        menuItemPosition: 2.0
        menuModelName: contextMenu.menu_model_name
        onActivated: clipboard.text = theSessionData.get(sessionSelectionModel.selectedIndexes[0], "nameRole")
        panelContext: contextMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 0.9
        menuModelName: contextMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Rename ..."
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 1.0
        menuModelName: contextMenu.menu_model_name
        property var targetIdx
        onActivated: {
            if(sessionSelectionModel.selectedIndexes.length != 1) {
                dialogHelpers.errorDialogFunc(
                    "Rename Playlist ...",
                    "Please select a single item to rename."
                    )
            } else {
                targetIdx = sessionSelectionModel.selectedIndexes[0]
                let name = theSessionData.get(targetIdx, "nameRole")
                let type = theSessionData.get(targetIdx, "typeRole")
                dialogHelpers.textInputDialog(
                    rename,
                    "Rename " + type,
                    "Enter a new name for the " + type,
                    name,
                    ["Cancel", "Rename " + type])
            }
        }
        panelContext: contextMenu.panelContext
        function rename(new_name, button) {
            if (button == "Cancel") return
            theSessionData.set(targetIdx, new_name, "nameRole")
        }
    }

    XsMenuModelItem {
        text: "Duplicate"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 2.0
        menuModelName: contextMenu.menu_model_name
        onActivated: {
            for (var i = 0; i < sessionSelectionModel.selectedIndexes.length; ++i) {
                let index = sessionSelectionModel.selectedIndexes[i]
                theSessionData.duplicateRows(index.row, 1, index.parent)
            }
        }
        panelContext: contextMenu.panelContext
    }


    XsMenuModelItem {
        text: "Combine"
        panelContext: contextMenu.panelContext
        menuModelName: contextMenu.menu_model_name
        menuPath: ""
        menuItemPosition: 3.0
        onActivated: {
            if(sessionSelectionModel.selectedIndexes.length) {
                theSessionData.mergeRows(sessionSelectionModel.selectedIndexes)
            }
        }
    }

    XsMenuModelItem {
        text: "Selected Playlists..."
        panelContext: contextMenu.panelContext
        menuModelName: contextMenu.menu_model_name
        menuPath: "Export"
        menuItemPosition: 4.0
        onActivated: {
            file_functions.saveSelectionNewPath(undefined)
        }
    }

    XsMenuModelItem {
        text: "Selected Sequence as OTIO ..."
        menuItemPosition: 4.5
        panelContext: contextMenu.panelContext
        menuModelName: contextMenu.menu_model_name
        menuPath: "Export"
        onActivated: {
            file_functions.exportSequencePath(function(result){if(result) {dialogHelpers.errorDialogFunc("Export Sequence Succeeded", "OTIO Exported")} else {dialogHelpers.errorDialogFunc("Export Sequence Failed", result)} })
        }
    }

    XsMenuModelItem {
        text: "Snippet"
        menuItemType: "divider"
        menuItemPosition: 8
        menuPath: ""
        menuModelName: contextMenu.menu_model_name
    }

    Repeater {
        model: DelegateModel {
            model: embeddedPython.playlistMenuModel
            delegate: Item {XsMenuModelItem {
                text: nameRole
                menuPath: menuPathRole
                menuItemPosition: (index*0.01)+8
                menuModelName: contextMenu.menu_model_name
                onActivated: embeddedPython.pyEvalFile(scriptPathRole)
            }}
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 20
        menuModelName: contextMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Remove Selected"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 21
        menuModelName: contextMenu.menu_model_name
        onActivated: removeSelected()
        panelContext: contextMenu.panelContext
    }

    XsMenuModelItem {
        text: "Remove Unused Media"
        menuItemType: "button"
        menuPath: "Cleanup"
        menuItemPosition: 32
        menuModelName: contextMenu.menu_model_name
        onActivated: {
            for (var idx = 0; idx < sessionSelectionModel.selectedIndexes.length; ++idx) {
                if (theSessionData.get(sessionSelectionModel.selectedIndexes[idx], "typeRole") == "Playlist") {
                    theSessionData.purgePlaylist(sessionSelectionModel.selectedIndexes[idx])
                }
            }
        }
        panelContext: contextMenu.panelContext
    }
}
