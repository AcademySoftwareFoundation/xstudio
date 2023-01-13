// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5

import xStudio 1.0

XsMenu {
    title: qsTr("Layout")
    id: viewMenu

    XsMenuSeparator {mytext:"Session Window"}
    XsMenuItem {
        mytext: qsTr("Browse")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Sort")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Timeline")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Assemble")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Trim")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Review")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Save Layout...")
        enabled: false
    }
    XsMenuSeparator {mytext:"Viewer Window"}
}





