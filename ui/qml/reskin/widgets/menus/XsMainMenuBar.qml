import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle {

    height: XsStyleSheet.menuHeight
    color: XsStyleSheet.menuBarColor
    id: menu_bar

    // this gives us access to the global tree model that defines menus, 
    // sub-menus and menu items
    XsMenusModel {
        id: menus_model
        modelDataName: "main menu bar"
        onJsonChanged: {
            root_index = index(-1, -1)
        }
    }

    // This index points us to the 'main menu bar' branch of
    // the global tree model
    property var root_index: menus_model.index(-1, -1)

    XsMenuModelItem {
        text: "Do Something"
        menuPath: "Session|Something|Something Else"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        hotkey: "Ctrl+Z"
        onActivated: {
            console.log("Clicke on File~Load")
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "Session|Something|Something Else"
        menuItemPosition: 3
        menuModelName: "main menu bar"
    }



    XsMenuModelItem {
        text: "Load"
        menuPath: "File"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            console.log("Clicke on File~Load")
        }
    }

    XsMenuModelItem {
        text: "Save"
        menuPath: "File"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        hotkey: "Ctrl+S"
        onActivated: {
            console.log("Well I never!")
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 3
        menuModelName: "main menu bar"
    }

    XsMenuModelItem {
        text: "Quit"
        menuPath: "File"
        menuItemPosition: 4
        menuModelName: "main menu bar"
        onActivated: {
            console.log("Well I never!")
        }
    }

    XsMenuModelItem {
        text: "New"
        menuPath: "Playlists"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            console.log("Well I never!")
        }
    }

    XsMenuModelItem {
        text: "Publish All"
        menuPath: "Playlists|Publish"
        menuItemPosition: 1
        hotkey: "Ctrl+P"
        menuModelName: "main menu bar"
        onActivated: {
            console.log("Well I never!")
        }
    }

    XsMenuModelItem {
        text: "Publish Selected"
        menuPath: "Playlists|Publish"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            console.log("Well I never!")
        }
    }

    XsMenuModelItem {
        text: "Colour Management"
        menuItemType: "toggle"
        menuPath: "Colour"
        menuItemPosition: 1
        isChecked: false
        menuModelName: "main menu bar"
        onIsCheckedChanged: {
            console.log("isChecked", isChecked)
        }
    }

    XsMenuModelItem {
        text: "Channels"
        menuPath: "Colour"
        menuItemType: "multichoice"
        menuItemPosition: 1
        choices: ["RGB", "R", "G", "B", "A", "Luminance"]
        currentChoice: "RGB"
        menuModelName: "main menu bar"
        onCurrentChoiceChanged: {
            console.log("currentChoice", currentChoice)
        }
    }

    ListView {

        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: 0 //10
        snapMode: ListView.SnapToItem

        model: DelegateModel {           

            model: menus_model
            rootIndex: root_index

            delegate: XsMenuItemNew { 
                
                menu_model: menus_model

                // As we loop over the top level items in the 'main menu bar' 
                // here, we set the index to row=index, column=0. This takes
                // us one step deeper into the tree on each iteration
                menu_model_index: menus_model.index(index, 0, root_index)

                // This var simply tells the pop-up to appear below the menu
                // item rather than to the right of it.
                is_in_bar: true

                parent_menu: menu_bar

            }

        }

    }

}