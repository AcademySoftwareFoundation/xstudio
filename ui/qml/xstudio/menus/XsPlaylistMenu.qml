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
            //shortcut: "Shift+P"
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewPlaylistDialog.qml", playlist_panel).open()
        }

        XsMenuItem {mytext: qsTr("Session &Divider")
            shortcut: "Shift+D"
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewSessionDividerDialog.qml", playlist_panel).open()
        }

        XsMenuItem {mytext: qsTr("Session Dated &Divider")
            shortcut: "Alt+D"
            onTriggered: {
                let d = XsUtils.openDialog("qrc:/dialogs/XsNewSessionDividerDialog.qml",playlist_panel)
                d.dated = true
                d.open()
            }
        }

        XsMenuItem {mytext: qsTr("&Subset")
            shortcut: "Shift+S"
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewSubsetDialog.qml", playlist_panel).open()
        }

        XsMenuItem {mytext: qsTr("&Timeline")
            shortcut: "Shift+T"
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewTimelineDialog.qml", playlist_panel).open()
            enabled: false
        }

        XsMenuItem {mytext: qsTr("&Contact Sheet")
            shortcut: "Shift+C"
            enabled: false
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewContactSheetDialog.qml", playlist_panel).open()
        }
        XsMenuItem {mytext: qsTr("D&ivider")
            shortcut: "Shift+i"
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewPlaylistDividerDialog.qml", playlist_panel).open()
        }
    }
    XsMenuItem {
        mytext: qsTr("Combine Selected Playlists")
        onTriggered: {
            app_window.session.mergePlaylists(XsUtils.getSelectedCuuids(app_window.session))
        }
    }


    XsMenuItem {mytext: qsTr("Export Selected Playlists...")
        onTriggered: {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSelectedSessionDialog.qml")
            dialog.open()
        }
    }

    XsMenuItem {mytext: qsTr("Remove Duplicate Media")
        enabled: false
    }

    XsFlagMenu {
        showChecked: false
        onFlagSet: app_window.session.flag_selected_items(hex, text)
    }

    XsButtonDialog {
        id: removePlaylists
        // parent: sessionWidget.playlist_panel
        text: "Remove Selected Playlists"
        width: 300
        buttonModel: ["Cancel", "Remove"]
        onSelected: {
            if(button_index == 1) {
                app_window.session.remove_selected_items()
            }
        }
    }

    XsMenuItem {
        mytext: qsTr("Remove Selected Playlists")
        onTriggered: removePlaylists.open()
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
        model: session.dataSources
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
