// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import ".."

XsPopupMenu {
    id: fileContextMenu
    property string itemPath: ""
    menu_model_name: "fbCtxMenu" + fileContextMenu

    Clipboard {
      id: clipboard
    }

    XsMenuModelItem {
        text: "Add To New Playlist"
        menuItemPosition: 1
        menuPath: ""
        onActivated: sendCommand({"action": "add_to_new_playlist", "paths": selectedItemsPaths})
        menuModelName: fileContextMenu.menu_model_name

    }
    XsMenuModelItem {
        text: "Add To Current Playlist"
        menuItemPosition: 2
        menuPath: ""
        onActivated: sendCommand({"action": "append_media", "paths": selectedItemsPaths})
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Compare"
        menuItemPosition: 3
        menuPath: ""
        onActivated: {
            sendCommand({"action": "compare_with_current_media", "paths": selectedItemsPaths})
        }
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Replace"
        menuItemPosition: 4
        menuPath: ""
        onActivated: {
            sendCommand({"action": "replace_current_media", "paths": selectedItemsPaths})
        }
        menuModelName: fileContextMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Reveal On Disk ..."
        menuPath: ""
        menuItemPosition: 5
        onActivated: {
            if (itemPath) {
                helpers.showURIS([helpers.QUrlFromPosixPath(itemPath)])
            } else if (selectedItemsPaths.length > 0) {
                helpers.showURIS([helpers.QUrlFromPosixPath(selectedItemsPaths[0])])
            }
        }
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Copy Selected File Names"
        menuPath: ""
        menuItemPosition: 6
        onActivated: {
            var v = []
            for (var i = 0; i < selectedItemsPaths.length; ++i) {
                var path = selectedItemsPaths[i]
                var fileName = helpers.QUrlFromPosixPath(path).fileName
                v.push(fileName)
            }
            clipboard.text = v.join("\n")
        }
        menuModelName: fileContextMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Copy Selected File Paths"
        menuPath: ""
        menuItemPosition: 7
        onActivated: {
            clipboard.text = selectedItemsPaths.join("\n")
        }
        menuModelName: fileContextMenu.menu_model_name
    }

}
