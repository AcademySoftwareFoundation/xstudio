// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.0

import xstudio.qml.clipboard 1.0
import xStudio 1.0


XsMenu {
    title: qsTr("File")
    XsMenuItem {
        mytext: qsTr("&New Session")
        shortcut: "Ctrl+N"
        onTriggered: app_window.session.save_before_new_session()
    }
    XsMenuItem {
        mytext: qsTr("Open Session...")
          shortcut: "Ctrl+O"
          onTriggered: app_window.session.save_before_open()
    }

    XsMenuItem {
        mytext: qsTr("Import Session...")
        onTriggered: app_window.session.import_session()
    }

    XsMenu {
      title: qsTr("Recent")
      Repeater {
        model: preferences.recent_history.value
        XsMenuItem {
            mytext: modelData
            onTriggered: app_window.session.save_before_open(modelData)
        }
      }
    }

    XsMenuItem {
        mytext: qsTr("Save Session")
        shortcut: "Ctrl+S"
        onTriggered: app_window.session.save_session()
    }
    XsMenuItem {
        mytext: qsTr("Save Session As...")
        shortcut: "Ctrl+Shift+S"
        onTriggered: app_window.session.save_session_as()
    }

    XsMenuSeparator {}
    XsMenuItem {
        mytext: qsTr("Add Media...")
        onTriggered: XsUtils.openDialogPlaylist("qrc:/dialogs/XsAddMediaDialog.qml").open()
    }
    XsMenuItem {
        mytext: qsTr("Add Media From Clipboard")
        onTriggered: app_window.session.add_media_from_clipboard()
    }
    XsMenuSeparator {}

    XsMenu {
          title: "Export"
          fakeDisabled: false
          XsMenuItem {
              mytext: qsTr("Selected Playlists As Session...")
              onTriggered: {
                  var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSelectedSessionDialog.qml")
                  dialog.open()
              }
          }
          XsMenuItem {
              mytext: qsTr("Notes As CSV...")
              onTriggered: app_window.session.export_notes_csv()
          }
          XsMenuItem {
              mytext: qsTr("Notes + Drawings...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("CDL...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("EDL...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("OTIO...")
              enabled: false
          }
    }
    XsMenu {
          title: "Export Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("Current Timeline Sequence....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Media Items...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Snapshot Current Frame...")
              enabled: false
          }
    }
    XsMenu {
          title: "Gather Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("Selected Playlists....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Media...")
              enabled: false
          }
    }
    XsMenu {
          title: "Relink Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("All Missing Files....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Missing Files...")
              enabled: false
          }
    }
    XsMenuItem {
        mytext: qsTr("Copy Session Link")
        enabled: true
        onTriggered: app_window.session.copy_session_link()
    }
    XsMenuItem {
        mytext: qsTr("Refresh All Metadata")
        enabled: false
    }

    XsMenuSeparator {}
    XsMenuItem {
        mytext: qsTr("&Quit")
        shortcut: "Ctrl+Q"
        onTriggered: {
          app_window.close()
        }
    }
}
