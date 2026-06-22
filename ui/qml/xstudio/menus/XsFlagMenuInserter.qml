// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQml 2.14

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: flag_menu_inserter

    property string text: "Flag Media"

    signal flagSet(string flag, string flag_text)

    ListModel {
        id: colours_model

        ListElement {
            name: "Remove Flag"
            colour: "#00000000"
        }
        ListElement {
            name: "Red"
            colour: "#FFFF0000"
        }
        ListElement {
            name: "Green"
            colour: "#FF00FF00"
        }
        ListElement {
            name: "Blue"
            colour: "#FF0000FF"
        }
        ListElement {
            name: "Yellow"
            colour: "#FFFFFF00"
        }
        ListElement {
            name: "Orange"
            colour: "#FFFFA500"
        }
        ListElement {
            name: "Purple"
            colour: "#FF800080"
        }
        ListElement {
            name: "Black"
            colour: "#FF000000"
        }
        ListElement {
            name: "White"
            colour: "#FFFFFFFF"
        }
    }

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
        model: colours_model
        Item {
            XsMenuModelItem {
                id: menu_item
                text: name
                menuItemType: "custom"
                menuPath: fullMenuPath
                menuItemPosition: index
                menuModelName: flag_menu_inserter.menuModelName
                userData: colour
                onActivated: {
                    flagSet(colour, name != "Remove Flag" ? name : "")
                }
                panelContext: flag_menu_inserter.panelContext
                customMenuQml: "import xStudio 1.0; XsFlagMenuItem {}"
            }
        }
    }
}
