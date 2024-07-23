// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    title: qsTr("Timeline")
    XsMenuItem {
        mytext: qsTr("New Sequence")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Duplicate Sequence")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Loop Selected")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Focus Mode")
        enabled: false
    }

    XsFlagMenu {
        showChecked: false
        onFlagSet: app_window.flagSelectedItems(hex)
    }

    XsMenu {
          title: "Tracks"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("New Video Track")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("New Audio Track")
              enabled: false
          }
          XsMenuSeparator {}
          XsMenuItem {
              mytext: qsTr("Group Selected")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Ungroup Selected")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Duplicate Selected")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Merge Selected")
              enabled: false
          }
          XsMenuSeparator {}
          XsMenuItem {
              mytext: qsTr("Select Track Clips")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Mute/Unmute")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Solo/Unsolo")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Rename")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Remove")
              enabled: false
          }
    }
    XsMenu {
          title: "Clips"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("Add To New Track")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Uniquify Clip Media")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Duplicate")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Remove")
              enabled: false
          }
    }

    XsMenu {
          title: "Import"
          XsMenuItem {
              mytext: qsTr("OTIO Sequence...")
              onTriggered: app_window.sessionFunction.importSequenceRequest()
          }
          // XsMenuItem {
          //     mytext: qsTr("Avid AFF...")
          //     enabled: false
          // }
          // XsMenuItem {
          //     mytext: qsTr("XML...")
          //     enabled: false
          // }
    }
    XsMenu {
          title: "Export"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("OTIO Sequence...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Avid AFF...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("XML...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("EDL...")
              enabled: false
          }
    }

    XsMenuItem {
        mytext: qsTr("Remove Sequence")
        enabled: false
    }

}
