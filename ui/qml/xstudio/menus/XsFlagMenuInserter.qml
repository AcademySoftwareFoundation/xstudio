// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQml 2.14

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: flag_menu_inserter

    property string text: "Flag Selected Media"

    signal flagSet(string flag, string flag_text)

    property var menuModelName
    property var panelContext: null
    property var menuPath: ""
    property var fullMenuPath: menuPath + (menuPath == "" ? text : "|" + text )
    property var menuPosition: 0.0

    onMenuPositionChanged: setMenuPosition()
    onFullMenuPathChanged: setMenuPosition()
    onMenuModelNameChanged: setMenuPosition()

    function setMenuPosition() {
        if (repeater.count && fullMenuPath != "")
            helpers.setMenuPathPosition(fullMenuPath, menuModelName, menuPosition)
    }

    Component.onCompleted: {
        setMenuPosition()
    }

    Repeater {
        id: repeater
        model: flagColours
        Item {
            XsMenuModelItem {
                id: menu_item
                text: modelData.name
                menuItemType: "custom"
                menuPath: fullMenuPath
                menuItemPosition: index
                menuModelName: flag_menu_inserter.menuModelName
                userData: modelData.colour
                onActivated: {
                    flagSet(modelData.colour, modelData.name != "Remove Flag" ? modelData.name : "")
                }
                panelContext: flag_menu_inserter.panelContext
                customMenuQml: "import xStudio 1.0; XsFlagMenuItem {}"
            }
        }
    }
}
