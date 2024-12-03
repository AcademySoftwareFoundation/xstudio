import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

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
            delegate: Item {XsMenuModelItem {
                text: nameRole
                menuPath: menuPathRole
                menuItemPosition: index+2
                menuModelName: "main menu bar"
                onActivated: embeddedPython.pyEvalFile(scriptPathRole)
            }}
        }
    }
}