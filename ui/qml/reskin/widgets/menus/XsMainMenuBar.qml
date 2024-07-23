// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle {

    id: menu_bar
    height: XsStyleSheet.menuHeight
    // color: XsStyleSheet.menuBarColor
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter( XsStyleSheet.menuBarColor, 1.15) }
        GradientStop { position: 1.0; color: Qt.darker( XsStyleSheet.menuBarColor, 1.15) }
    }

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

    // XsMenuModelItem {
    //     text: "Save Session"
    //     menuPath: "Session|Current Session"
    //     menuItemPosition: 1
    //     menuModelName: "main menu bar"
    //     hotkey: "Ctrl+Z"
    //     onActivated: {
    //     }
    // }

    // XsMenuModelItem {
    //     menuItemType: "divider"
    //     menuPath: ""
    //     menuItemPosition: 3
    //     menuModelName: "main menu bar"
    // }

    XsMenuModelItem {
        text: "New Session"
        menuPath: "File"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }
    XsMenuModelItem {
        text: "Open Session"
        menuPath: "File"
        menuItemPosition: 2
        menuModelName: "main menu bar"
        onActivated: {
            var component = Qt.createComponent("qrc:/widgets/dialogs/XsOpenSessionDialog.qml");
            if (component.status == Component.Ready) {
                var dialog = component.createObject(parent)
                dialog.open()
            } else {
                console.log("Error loading component:", component.errorString());
            }
        }
    }
    XsMenuModelItem {
        text: "Save Session"
        menuPath: "File"
        menuItemPosition: 3
        menuModelName: "main menu bar"
        onActivated: {
        }
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "File"
        menuItemPosition: 4
        menuModelName: "main menu bar"
    }
    XsMenuModelItem {
        text: "Quit"
        menuPath: "File"
        menuItemPosition: 5
        menuModelName: "main menu bar"
        onActivated: {
            Qt.quit()
        }
    }

    XsMenuModelItem {
        text: "Cut"
        menuPath: "Edit"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "New"
        menuPath: "Playlists"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Flag Media"
        menuPath: "Media"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "New Sequence"
        menuPath: "Timeline"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Play/Pause"
        menuPath: "Playback"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Hide UI"
        menuPath: "Viewer"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Save Layout.."
        menuPath: "Layout"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Drawing Tools"
        menuPath: "Panels"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: "Panels"
        menuModelName: "main menu bar"
    }
    XsMenuModelItem {
        text: "Red"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(3).value
        }
    }
    XsMenuModelItem {
        text: "Orange"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(4).value
        }
    }
    XsMenuModelItem {
        text: "Yellow"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(5).value
        }
    }
    XsMenuModelItem {
        text: "Green"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(6).value
        }
    }
    XsMenuModelItem {
        text: "Blue"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(0).value
        }
    }
    XsMenuModelItem {
        text: "Purple"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(1).value
        }
    }
    XsMenuModelItem {
        text: "Pink"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(2).value
        }
    }
    XsMenuModelItem {
        text: "Graphite"
        menuPath: "Panels|Settings|UI Accent Colour"
        menuModelName: "main menu bar"
        onActivated: {
            XsStyleSheet.accentColor = accentColorModel.get(7).value
        }
    }


    XsMenuModelItem {
        text: "ShotGrid"
        menuPath: "Publish"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }

    XsMenuModelItem {
        text: "Publish All"
        menuPath: "Playlists|Publish"
        menuItemPosition: 1
        hotkey: "Ctrl+P"
        menuModelName: "main menu bar"
        onActivated: {
        }
    }


    XsMenuModelItem {
        text: "Bypass Colour Management"
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
        menuPath: ""
        menuItemType: "multichoice"
        menuItemPosition: 1
        choices: ["RGB", "R", "G", "B", "A", "Luminance"]
        currentChoice: "Luminance"
        menuModelName: "main menu bar"
        onCurrentChoiceChanged: {
            console.log("currentChoice", currentChoice)
        }
    }

    // XsMenuModelItem {
    //     text: "UI Accent Colour(WIP)"
    //     menuPath: "Colour"
    //     menuItemType: "multichoice"
    //     menuItemPosition: 1
    //     choices: ["Blue", "Purple", "Pink", "Red", "Orange", "Yellow", "Green", "Graphite"]
    //     currentChoice: "Orange"
    //     menuModelName: "main menu bar"
    //     onCurrentChoiceChanged: {
    //         console.log("currentChoice", currentChoice)
    //     }
    // }


    XsMenuModelItem {
        text: "About"
        menuPath: "Help"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
        }
    }


    ListModel { id: accentColorModel

        ListElement {
            name: qsTr("Blue")
            value: "#307bf6"
        }
        ListElement {
            name: qsTr("Purple")
            value: "#9b56a3"
        }
        ListElement {
            name: qsTr("Pink")
            value: "#e65d9c"
        }
        ListElement {
            name: qsTr("Red")
            value: "#ed5f5d"
        }
        ListElement {
            name: qsTr("Orange")
            value: "#e9883a"
        }
        ListElement {
            name: qsTr("Yellow")
            value: "#f3ba4b"
        }
        ListElement {
            name: qsTr("Green")
            value: "#77b756"
        }
        ListElement {
            name: qsTr("Graphite")
            value: "#999999"//"#666666"
        }

    }



    XsListView {

        anchors.fill: parent
        orientation: ListView.Horizontal
        isScrollbarVisibile: false

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