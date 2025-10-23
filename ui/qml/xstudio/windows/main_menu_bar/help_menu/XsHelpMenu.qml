import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

import "."

Item {

    Loader {
        id: loader
    }

    Component.onCompleted: {
        // ensure Help menu is the last (rightmost)
        helpers.setMenuPathPosition("Help", "main menu bar", 100)
    }

    Component {
        id: hotkeys_dialog
        XsHotkeysDialog {}
    }

    XsReleaseNotes {
        id: release_notes
    }

    Component {
        id: about_dialog
        XsAboutDialog {}
    }

    XsMenuModelItem {
        text: "User Documentation"
        menuPath: "Help"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            openDoc(studio.userDocsUrl())
        }
    }

    XsMenuModelItem {
        text: "Hotkeys"
        menuPath: "Help"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: {
            loader.sourceComponent = hotkeys_dialog
            loader.item.visible = true
        }
    }

    // Re-instate this when API docs look better!
    /*XsMenuModelItem {
        text: "Python/C++ API Documentation"
        menuPath: "Help"
        menuItemPosition: 3
        menuModelName: "main menu bar"
        onActivated: {
            openDoc(studio.apiDocsUrl())
        }
    }*/

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "Help"
        menuItemPosition: 4
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "View Release Notes"
        menuPath: "Help"
        menuItemPosition: 5
        menuModelName: "main menu bar"
        onActivated: {
            release_notes.showDialog()            
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "Help"
        menuItemPosition: 6
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "About"
        menuPath: "Help"
        menuItemPosition: 7
        menuModelName: "main menu bar"
        onActivated: {
            loader.sourceComponent = about_dialog
            loader.item.visible = true
        }
    }

    function openDoc(url) {
        if (url == "") {
            dialogHelpers.errorDialogFunc("Error", "Unable to resolve location of user docs.")
        } else {
            if(!helpers.openURL(url)) {
                dialogHelpers.errorDialogFunc("Error", "Failed to launch webbrowser, this is the link, please manually visit this.<BR><BR>"+url+"<BR><BR>")
            }
        }
    }
}