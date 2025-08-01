// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    id: playlist_menu
    title: qsTr("Playlists")
    property var playlist_panel: sessionWidget.playlist_panel

    XsMenu {
        title: "New"
        XsMenuItem {
            mytext: qsTr("&Playlist")
            shortcut: "Ctrl+Shift+P"
            onTriggered: sessionFunction.newPlaylist(
                app_window.sessionModel.index(0, 0), null
            )
        }

        XsMenuItem {mytext: qsTr("Session &Divider")
            shortcut: "Ctrl+Shift+D"
            onTriggered: sessionFunction.newDivider(
                app_window.sessionModel.index(0, 0), null, playlist_panel
            )
        }

        XsMenuItem {mytext: qsTr("Session Dated &Divider")
            shortcut: "Alt+D"
            onTriggered: sessionFunction.newDivider(
                app_window.sessionModel.index(0, 0), XsUtils.yyyymmdd("-"), playlist_panel
            )
        }

        XsMenuItem {mytext: qsTr("&Subset")
            shortcut: "Ctrl+Shift+S"
            onTriggered: {
                let ind = app_window.sessionFunction.firstSelected("Playlist")
                if(ind != null)  {
                    sessionFunction.newSubset(
                        ind.model.index(2,0,ind), null, playlist_panel
                    )
                }
            }
        }

        XsMenuItem {mytext: qsTr("&Timeline")
            shortcut: "Ctrl+Shift+T"
            enabled: false
        }

        XsMenuItem {mytext: qsTr("&Contact Sheet")
            shortcut: "Ctrl+Shift+C"
            enabled: false
        }

        XsMenuItem {mytext: qsTr("D&ivider")
            shortcut: "Ctrl+Shift+I"
            onTriggered: {
                let ind = app_window.sessionFunction.firstSelected("Playlist")
                if(ind != null)  {
                    sessionFunction.newDivider(
                        ind.model.index(2,0,ind), null, playlist_panel
                    )
                }
            }
        }
    }

    XsMenuItem {
        mytext: qsTr("Combine Selected Playlists")
        onTriggered: sessionFunction.mergeSelected()
    }


    XsMenuItem {mytext: qsTr("Export Selected Playlists...")
        onTriggered: app_window.sessionFunction.saveSelectedSessionDialog()
    }

    XsMenuItem {mytext: qsTr("Remove Duplicate Media")
        enabled: false
    }

    XsFlagMenu {
        showChecked: false
        onFlagSet: app_window.sessionFunction.flagSelected(hex)
    }

     XsMenuItem {
        mytext: qsTr("Remove Selected")
        onTriggered: app_window.sessionFunction.removeSelected()
    }

    XsMenuSeparator {
        id: data_source_menu
    }

    function getMenuIndex(menu, obj) {
         for(var i = 0; i < contentChildren.length; i++) {
            if(contentChildren[i] == obj) {
                return i
            }
         }
         return -1
    }

    Instantiator {
        model: studio.dataSources
        XsPluginMenu {
            qmlMenuString: modelData.qmlMenuString
            qmlName: modelData.qmlName
            backendId: modelData.backendId
        }
        onObjectAdded: playlist_menu.insertMenu(index + getMenuIndex(playlist_menu, data_source_menu), object.plugin_ui)
        onObjectRemoved: playlist_menu.removeMenu(object.plugin_ui)
    }

    XsModuleMenuBuilder {
        parent_menu: playlist_menu
        root_menu_name: "datasource_menu"
    }
}
