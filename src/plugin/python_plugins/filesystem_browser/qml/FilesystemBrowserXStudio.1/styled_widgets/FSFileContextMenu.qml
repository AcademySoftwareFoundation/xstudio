// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import ".."

XsPopupMenu {
    id: fileContextMenu
    property string itemPath: ""
    property var selectedPaths: []
    menu_model_name: "fbCtxMenu" + fileContextMenu
    XsMenuModelItem {
        text: "Replace"
        menuItemPosition: 1
        menuPath: ""
        onActivated: sendCommand({"action": "replace_current_media", "path": itemPath})
        menuModelName: fileContextMenu.menu_model_name

    }
    XsMenuModelItem {
        text: "Compare with"
        menuItemPosition: 2
        menuPath: ""
        onActivated: sendCommand({"action": "compare_with_current_media", "path": itemPath})
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Append to Playlist"
        menuItemPosition: 3
        menuPath: ""
        onActivated: {
            sendCommand({"action": "append_media", "paths": selectedPaths})
        }
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Copy Path"
        menuPath: ""
        onActivated: sendCommand({"action": "copy_path", "path": itemPath})
        menuModelName: fileContextMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Show in Finder"
        menuPath: ""
        onActivated: {
            helpers.showURIS([helpers.QUrlFromPosixPath(itemPath)])
        }
        menuModelName: fileContextMenu.menu_model_name
    }


}
