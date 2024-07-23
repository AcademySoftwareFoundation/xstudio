// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    id: menu
    property var playlist_widget: app_window.sessionWidget.playlist_panel.playlist_control
    title: qsTr("Media")

    property var visual_parent: menu.parent instanceof MenuBarItem ? app_window.sessionWidget.playlist_panel : menu.parent
    property alias revealMenu: reveal_menu
    property alias advancedMenu: advanced_menu

    XsFlagMenu {
        title: qsTr("Flag Media")
        showChecked: false
        onFlagSet: app_window.sessionFunction.flagSelectedMedia(hex, text)
    }

    XsMenu {
          title: "Add To New"
          fakeDisabled: false
          XsMenuItem {
              mytext: qsTr("Playlist...")
              onTriggered: {
                app_window.sessionFunction.addMediaToNewPlaylist()
              }
          }
          XsMenuItem {
              mytext: qsTr("Subset...")
              onTriggered: {
                app_window.sessionFunction.addMediaToNewSubset()
              }
          }
          XsMenuItem {
              enabled: false
              mytext: qsTr("Contact Sheet...")
          }

          XsMenuItem {
              mytext: qsTr("Timeline...")
              onTriggered: {
                app_window.sessionFunction.addMediaToNewTimeline()
              }
          }
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Select All")
        shortcut: "Ctrl+A"
        onTriggered: app_window.sessionFunction.selectAllMedia()
    }

    XsMenuItem {
        mytext: qsTr("Deselect All")
        shortcut: "Ctrl+D"
        onTriggered: app_window.sessionFunction.deselectAllMedia()
    }

    XsMenuItem {
        mytext: qsTr("Sort Media Alphabetically")
        enabled: true
        onTriggered: app_window.sessionFunction.sortAlphabetically()
    }

    XsMenuSeparator {}

    XsMenu {
        title: "Copy"
        fakeDisabled: false
        XsMenuItem {
            mytext: qsTr("Selected File Names")
            onTriggered: app_window.sessionFunction.copyMediaFileName(true)
        }
        XsMenuItem {
            mytext: qsTr("Selected File Paths")
            onTriggered: app_window.sessionFunction.copyMediaFilePath(true)
        }
        XsMenuItem {
            mytext: qsTr("Copy Selected To Email Link")
            onTriggered: app_window.sessionFunction.copyMediaToLink()
        }
        XsMenuItem {
            mytext: qsTr("Copy Selected To Shell Command")
            onTriggered: app_window.sessionFunction.copyMediaToCmd()
        }
    }

    XsMenuItem {
        mytext: qsTr("Duplicate Media")
        onTriggered: app_window.sessionFunction.duplicateSelectedMedia()
    }

    XsMenu {
        title: "Reveal Source"
        id: reveal_menu

        XsMenuItem {
            mytext: qsTr("On Disk...")
            onTriggered: app_window.sessionFunction.revealSelectedSources()
        }
    }

    // XsMenuSeparator {}
    // XsMenu {

    //     id: conform_menu
    //     title: "Conform"
    //     fakeDisabled: false
    //     Repeater {
    //         model: app_window.sessionModel.conformTasks
    //         onItemAdded: conform_menu.insertItem(index, item)
    //         onItemRemoved: conform_menu.removeItem(item)

    //         XsMenuItem {
    //             mytext: modelData
    //             enabled: true
    //             onTriggered: {
    //                 app_window.sessionFunction.conformInsertSelectedMedia(modelData)
    //             }
    //         }
    //     }
    // }

    XsMenuSeparator {}

    XsMenu {

        id: stream_menu
        title: "Select Image Layer/Stream"
        fakeDisabled: false
        Repeater {
            model: app_window.mediaImageSource.streams
            onItemAdded: stream_menu.insertItem(index, item)
            onItemRemoved: stream_menu.removeItem(item)

            XsMenuItem {
                mytext: modelData.name
                enabled: true
                onTriggered: app_window.mediaImageSource.values.imageActorUuidRole = modelData.uuid
                mycheckable: true
                checked: app_window.mediaImageSource.values.imageActorUuidRole == modelData.uuid
            }
        }
    }

    XsMenuSeparator {}

    XsMenu {
        id: advanced_menu

        title: "Advanced"
        fakeDisabled: false

        XsMenuItem {
            mytext: qsTr("Refresh Selected Metadata")
            enabled: false
        }

        XsMenuItem {
            mytext: qsTr("Clear Cache")
            enabled: true
            onTriggered: studio.clearImageCache()
        }

        XsMenuItem {
            mytext: qsTr("Clear Selected Media From Cache")
            enabled: true
            onTriggered: app_window.sessionFunction.clearMediaFromCache()
        }

        XsMenuItem {
            mytext: qsTr("Set Selected Media Rate")
            enabled: true
            onTriggered: app_window.sessionFunction.setMediaRateRequest()
        }

        XsMenuItem {
            mytext: qsTr("Load Additional Sources For Selected Media")
            enabled: true
            onTriggered: app_window.sessionFunction.gatherMediaForSelected()
        }
        XsMenuItem {
            mytext: qsTr("Relink Selected Media")
            enabled: true
            onTriggered: app_window.sessionFunction.relinkSelectedMedia()
        }
        XsMenuItem {
            mytext: qsTr("Decompose Selected Media")
            enabled: true
            onTriggered: app_window.sessionFunction.decomposeSelectedMedia()
        }
        XsMenuItem {
            mytext: qsTr("Reload Selected Media")
            enabled: true
            onTriggered: app_window.sessionFunction.rescanSelectedMedia()
        }
    }

    // XsMenuItem {
    //     mytext: qsTr("Add Media...")
    //     onTriggered: XsUtils.openDialogPlaylist("qrc:/dialogs/XsAddMediaDialog.qml", playlist_widget).open()
    // }

    // XsMenuSeparator {id:media_menu_plugin_sep}

    // XsModuleMenuBuilder {
    //     parent_menu: menu
    //     root_menu_name: "media_menu_plugin_items"
    //     insert_after: media_menu_plugin_sep
    // }

    XsModuleMenuBuilder {
        parent_menu: menu
        root_menu_name: "Plugins"
    }

    XsMenuSeparator {
        id: after_plugins_separator
        visible: !extras_menu.empty
        height: visible ? implicitHeight : 0
    }

    XsModuleMenuBuilder {
        id: extras_menu
        parent_menu: menu
        root_menu_name: "media_menu_extras"
        insert_after: after_plugins_separator
    }

    XsMenuSeparator {
    }

    // XsButtonDialog {
    //     id: removeMedia
    //     // parent: sessionWidget.media_list
    //     text: "Remove Selected Media"
    //     width: 300
    //     buttonModel: ["Cancel", "Remove"]
    //     onSelected: {
    //         if(button_index == 1) {
    //             sessionWidget.media_list.delete_selected()
    //         }
    //     }
    // }

    XsMenuItem {
        mytext: qsTr("Remove Selected Media")
        shortcut: "Del/Bspace"
        enabled: true

        onTriggered: app_window.sessionFunction.requestRemoveSelectedMedia()
    }

//    XsMenuItem {
//        mytext: qsTr("Remove Duplicate Media")
//        enabled: false
//    }
//    XsMenuItem {
//        mytext: qsTr("Snapshot Current Frame")
//        enabled: false
//    }
}
