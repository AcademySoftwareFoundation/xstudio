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

    XsFlagMenu {
        title: qsTr("Flag Media")
        onFlagSet: app_window.session.flag_selected_media(hex, text)
        colours_model: app_window.session.mediaFlags
    }

    // ctrl -[0-9] should effect selection ..
    //  add hotkeys...


    // add hotkeys if they exist

    // XsColorsModel {id: colorsModel}
    // XsMenu {
    //       title: "Flag"
    //       id: setFlagMenu
    //       fakeDisabled: true
    //       Instantiator {
    //           model: colorsModel
    //           onObjectAdded: setFlagMenu.insertItem( index, object )
    //           onObjectRemoved: setFlagMenu.removeItem( object )
    //           delegate: XsMenuItem {
    //               iconbg: value
    //               enabled: false
    //               mytext: name
    //               mycheckable: true
    //           }
    //       }
    //       XsMenuSeparator {}
    //       XsMenuItem {
    //           mytext: qsTr("Clear Flags")
    //           enabled: false
    //       }
    // }

    XsMenu {
          title: "Add To New"
          fakeDisabled: false
          XsMenuItem {
              mytext: qsTr("Playlist...")
              onTriggered: {
                app_window.session.add_media_to_new_playlist(visual_parent)
              }
          }
          XsMenuItem {
              mytext: qsTr("Subset...")
              onTriggered: {
                app_window.session.add_media_to_new_subset(visual_parent)
              }
          }
          XsMenuItem {
              enabled: false
              mytext: qsTr("Contact Sheet...")
              onTriggered: {
                  app_window.session.add_media_to_new_contact_sheet(visual_parent)
              }
          }

          XsMenuItem {
              mytext: qsTr("Timeline...")
              enabled: false
          }
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Select All")
        shortcut: "Ctrl+A"

        onTriggered: sessionWidget.media_list.selectAll();
    }

    XsMenuItem {
        mytext: qsTr("Deselect All")
        shortcut: "Ctrl+D"
        onTriggered: sessionWidget.media_list.deselectAll();
    }

    XsMenuItem {
        mytext: qsTr("Sort Media Alphabetically")
        enabled: true
        onTriggered: {
            if (session.playlist) {
                sessionWidget.media_list.sortAlphabetically()
            }
        }
    }

    XsMenuSeparator {}

    XsMenu {
        title: "Copy"
        fakeDisabled: false
        XsMenuItem {
            mytext: qsTr("Selected File Names")
            onTriggered: app_window.session.copy_media_file_name()
        }
        XsMenuItem {
            mytext: qsTr("Selected File Paths")
            onTriggered: app_window.session.copy_media_file_path()
        }
        XsMenuItem {
            mytext: qsTr("Copy Selected To Email Link")
            onTriggered: app_window.session.copy_media_to_link()
        }
        XsMenuItem {
            mytext: qsTr("Copy Selected To Shell Command")
            onTriggered: app_window.session.copy_media_to_cmd()
        }
    }

    XsMenuItem {
        mytext: qsTr("Duplicate Media")
        onTriggered: app_window.session.duplicate_selected_media()
    }

    XsMenuItem {
        mytext: qsTr("Reveal Source File...")
        onTriggered: app_window.session.reveal_selected_sources()
    }

    XsMenuSeparator {}

    XsMenu {

        id: stream_menu
        title: "Select Stream"
        fakeDisabled: false
        property var fi: sessionWidget.media_list.firstMediaItemInSelection
        property var stream_model: fi ? fi.mediaSource ? fi.mediaSource.streams : [] : []
        Repeater {
            model: stream_menu.stream_model ? stream_menu.stream_model : []
            XsMenuItem {
                mytext: stream_menu.stream_model[index].name
                enabled: true
                extraLeftMargin: 20
                onTriggered: sessionWidget.media_list.switchSelectedToNamedStream(stream_menu.stream_model[index].name)
                mycheckable: true
                checked: sessionWidget.media_list.firstMediaItemInSelection.mediaSource.current == stream_menu.stream_model[index]
            }
        }
    }

    XsMenuSeparator {}

    XsMenu {
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
            onTriggered: sessionWidget.media_list.evict_selected()
        }

        XsMenuItem {
            mytext: qsTr("Set Selected Media Rate")
            enabled: true
            onTriggered: app_window.session.set_selected_media_rate(visual_parent)
        }

        XsMenuItem {
            mytext: qsTr("Load Additional Sources For Selected Media")
            enabled: true
            onTriggered: sessionWidget.media_list.gather_media_for_selected()
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
    XsMenuSeparator {}

    XsButtonDialog {
        id: removeMedia
        // parent: sessionWidget.media_list
        text: "Remove Selected Media"
        width: 300
        buttonModel: ["Cancel", "Remove"]
        onSelected: {
            if(button_index == 1) {
                sessionWidget.media_list.delete_selected()
            }
        }
    }

    XsMenuItem {
        mytext: qsTr("Remove Selected Media")
        shortcut: "Del/Bspace"
        enabled: true

        onTriggered: removeMedia.open()
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
