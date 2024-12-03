import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

import "."

Item {

    /**************************************************************

    HOTKEYS

    ****************************************************************/
    XsHotkey {
        id: new_session_hotkey
        sequence: "Ctrl+N"
        name: "New Session"
        description: "Creates a new, empty xSTUDIO session"
    }

    XsHotkey {
        id: open_session_hotkey
        sequence: "Ctrl+O"
        name: "Open Session"
        description: "Opens a file dialog to find and load an xSTUDIO session file"
    }

    XsHotkey {
        id: save_session_hotkey
        sequence: "Ctrl+S"
        name: "Save Session"
        description: "Saves the current session to disk using the last loaded or saved file path."
        onActivated: {

        }
    }

    XsHotkey {
        id: save_session_as_hotkey
        sequence: "Ctrl+Shift+S"
        name: "Save Session As"
        description: "Opens a file dialog to choose a file path to save the current xSTUDIO session to."
    }

    XsHotkey {
        id: quit_session_hotkey
        sequence: "Ctrl+Q"
        name: "Quit"
        description: "Closes the xSTUDIO session and application."
    }

    /**************************************************************

    MENU ITEMS

    ****************************************************************/

    XsMenuModelItem {
        text: "New Session"
        menuPath: "File"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        hotkeyUuid: new_session_hotkey.uuid
        onActivated: {
            file_functions.newSessionWithCheck()
        }
        onHotkeyActivated: {
            file_functions.newSessionWithCheck()
        }
    }

    XsMenuModelItem {
        text: "Open Session"
        menuPath: "File"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        hotkeyUuid: open_session_hotkey.uuid
        onActivated: {
            file_functions.loadSessionWithCheck()
        }
        onHotkeyActivated: {
            file_functions.loadSessionWithCheck()
        }

    }

    XsMenuModelItem {
        id: me
        text: "Import Session ..."
        menuPath: "File"
        menuItemPosition: 3
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.importSession()
        }

    }

    XsMenuModelItem {
        text: "Import OTIO ..."
        menuPath: "File"
        menuItemPosition: 3.1
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.importOTIO()
        }
    }

    Repeater {
        model: file_functions.recentFiles
        Item {
            XsMenuModelItem {
                text: file_functions.recentFiles[index]
                menuPath: "File|Recent"
                menuItemPosition: index
                menuModelName: "main menu bar"
                onActivated: {
                    file_functions.doLoadSession(file_functions.recentFiles[index])
                }
                Component.onCompleted: {
                    setMenuPathPosition("File|Recent", 3.4)
                }

            }
        }
    }

    XsMenuModelItem {
        text: "Save Session"
        menuPath: "File"
        menuItemPosition: 3.5
        menuModelName: "main menu bar"
        hotkeyUuid: save_session_hotkey.uuid
        onActivated: {
            file_functions.saveSessionCheck(undefined)
        }
        onHotkeyActivated: {
            file_functions.saveSessionCheck(undefined)
        }
    }

    XsMenuModelItem {
        text: "Save Session As ..."
        menuPath: "File"
        menuItemPosition: 3.6
        menuModelName: "main menu bar"
        hotkeyUuid: save_session_as_hotkey.uuid
        onActivated: {
            file_functions.saveSessionNewPath()
        }
        onHotkeyActivated: {
            file_functions.saveSessionNewPath()
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 4
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "Add Media ..."
        menuPath: "File"
        menuItemPosition: 5
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.loadMedia(undefined)
        }
    }

    XsMenuModelItem {
        text: "Add Media From Clipboard"
        menuPath: "File"
        menuItemPosition: 6
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.addMediaFromClipboard()
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 7
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "Selected Playlists as Session ..."
        menuPath: "File|Export"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.saveSelectionNewPath(undefined)
        }
        Component.onCompleted: {
            setMenuPathPosition("File|Export", 7.5)
        }
    }

    XsMenuModelItem {
        text: "Selected Sequence as OTIO ..."
        menuPath: "File|Export"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.exportSequencePath(function(result){if(result) {dialogHelpers.errorDialogFunc("Export Sequence Succeeded", "OTIO Exported")} else {dialogHelpers.errorDialogFunc("Export Sequence Failed", result)} })
        }
        Component.onCompleted: {
            setMenuPathPosition("File|Export", 7.6)
        }
    }

    XsMenuModelItem {
        text: "Notes as CSV ..."
        menuPath: "File|Export"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.exportNotedToCSV()
        }
    }

    XsMenuModelItem {
        text: "Copy Session Link"
        menuPath: "File"
        menuItemPosition: 8
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.copySessionLink()
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 9
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "Preferences Manager ..."
        menuPath: "File|Preferences"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        menuItemType: "button"
        onActivated: {
            prefs_loader.sourceComponent = prefs_dialog
            prefs_loader.item.visible = true
        }
        Component.onCompleted: {
            setMenuPathPosition("File|Preferences", 10.0)
        }

    }

    XsMenuModelItem {
        text: "Hotkeys Manager ..."
        menuPath: "File|Preferences"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        menuItemType: "button"
        onActivated: {
            prefs_loader.sourceComponent = hotkeys_dialog
            prefs_loader.item.visible = true
        }
    }

    Loader {
        id: prefs_loader
    }

    Component {
        id: prefs_dialog
        XsPreferencesDialog {}
    }

    Component {
        id: hotkeys_dialog
        XsHotkeysDialog {}
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 11
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "Quit"
        menuPath: "File"
        menuItemPosition: 12
        hotkeyUuid: quit_session_hotkey.uuid
        menuModelName: "main menu bar"
        onActivated: {
            file_functions.quitWithCheck()
        }
        onHotkeyActivated: {
            file_functions.quitWithCheck()
        }

    }

}