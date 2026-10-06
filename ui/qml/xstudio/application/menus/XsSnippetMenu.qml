import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

Item {
    XsMenuModelItem {
        text: "Reload"
        menuPath: "Snippets"
        menuItemPosition: 0
        menuModelName: "main menu bar"
        onActivated: embeddedPython.reloadSnippets()
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "Snippets"
        menuItemPosition: 1
        menuModelName: "main menu bar"
    }

    Repeater {
        model: DelegateModel {
            model: embeddedPython.applicationMenuModel
            delegate: Item {
                XsHotkey {
                    id: hk
                    name: nameRole
                    description: nameRole
                    componentName: "Global Snippets"
                    onActivated: embeddedPython.pyEvalFile(scriptPathRole)
                    sequence: menuHotKeyRole || null
                }

                XsMenuModelItem {
                    hotkeyUuid: hk.uuid
                    text: nameRole
                    menuPath: menuPathRole
                    menuItemPosition: index+2
                    menuModelName: "main menu bar"
                    onActivated: embeddedPython.pyEvalFile(scriptPathRole)
                }
            }
        }
    }
}