// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

XsPopupMenu {

    property var viewport

    id: btnMenu
    visible: false
    menu_model_name: viewport.name + "_context_menu"

    /**************************************************************

    Static Menu Items (most items in this menu are added dymanically
        from the backend - e.g. PlayheadActor, Viewport classes do this)

    ****************************************************************/

    // XsMenuModelItem {
    //     text: isPopoutViewer ? "" : "Presentation Mode"
    //     menuItemType: "toggle"
    //     menuPath: ""
    //     menuItemPosition: 1
    //     menuModelName: btnMenu.menu_model_name
    //     hotkeyUuid: presentation_mode_hotkey.uuid
    //     // awkward two way binding!
    //     isChecked: appWindow.layoutName == "Present"
    //     onActivated: {
    //         appWindow.togglePresentationMode()
    //     }
    //     panelContext: btnMenu.panelContext
    // }

    XsMenuModelItem {
        text: "Full Screen"
        menuItemType: "toggle"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: btnMenu.menu_model_name
        hotkeyUuid: fullscreen_hotkey.uuid
        isChecked: appWindow.fullscreen
        onActivated: {
            appWindow.fullscreen = !appWindow.fullscreen
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: "Hide UI"
        menuPath: ""
        menuItemType: "toggle"
        menuItemPosition: 2.5
        menuModelName: btnMenu.menu_model_name
        hotkeyUuid: hide_ui_hotkey.uuid
        isChecked: !viewportWidget.elementsVisible
        onActivated: {
            // ignore trigger from hotkey, where userData is the context
            viewportWidget.elementsVisible = !viewportWidget.elementsVisible
        }
        panelContext: btnMenu.panelContext
    }

    XsMenuModelItem {
        text: isPopoutViewer ? "" : "Menu Bar"
        enabled: viewportWidget.elementsVisible
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 1
        menuModelName: btnMenu.menu_model_name
        isChecked: viewportWidget.menuBarVisible
        onActivated: {
            viewportWidget.menuBarVisible = !viewportWidget.menuBarVisible
        }
        panelContext: btnMenu.panelContext
    }
    XsMenuModelItem {
        menuPath: "UI Elements"
        menuItemType: "divider"
        menuItemPosition: 2
        menuModelName: isPopoutViewer? "" : btnMenu.menu_model_name
    }
    XsMenuModelItem {
        text: isPopoutViewer ? "" : layoutName != "Present" ? "Tab Bar" : ""
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 3
        menuModelName: btnMenu.menu_model_name
        enabled: viewportWidget.elementsVisible && 
            (appWindow.tabsVisibility == 2 || 
                (appWindow.tabsVisibility == 1 && container.tabCount > 1))
        isChecked: viewportWidget.tabBarVisible
        onActivated: {
            viewportWidget.tabBarVisible = !viewportWidget.tabBarVisible
        }
        panelContext: btnMenu.panelContext
    }
    XsMenuModelItem {
        text: "Action Bar"
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 4
        menuModelName: btnMenu.menu_model_name
        enabled: viewportWidget.elementsVisible
        isChecked: viewportWidget.actionBarVisible
        onActivated: {
            viewportWidget.actionBarVisible = !viewportWidget.actionBarVisible
        }
        panelContext: btnMenu.panelContext
    }
    XsMenuModelItem {
        text: "Info Bar"
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 5
        menuModelName: btnMenu.menu_model_name
        enabled: viewportWidget.elementsVisible
        isChecked: viewportWidget.infoBarVisible
        onActivated: {
            viewportWidget.infoBarVisible = !viewportWidget.infoBarVisible
        }
        panelContext: btnMenu.panelContext
    }
    XsMenuModelItem {
        text: "Tool Bar"
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 6
        menuModelName: btnMenu.menu_model_name
        enabled: viewportWidget.elementsVisible
        isChecked: viewportWidget.toolBarVisible
        onActivated: {
            viewportWidget.toolBarVisible = !viewportWidget.toolBarVisible
        }
        panelContext: btnMenu.panelContext
    }
    XsMenuModelItem {
        text: "Transport Bar"
        menuPath: "UI Elements"
        menuItemType: "toggle"
        menuItemPosition: 7
        menuModelName: btnMenu.menu_model_name
        enabled: viewportWidget.elementsVisible
        isChecked: viewportWidget.transportBarVisible
        onActivated: {
            viewportWidget.transportBarVisible = !viewportWidget.transportBarVisible
        }
        panelContext: btnMenu.panelContext
    }


    XsModuleData {
        id: toolbar_model_data
        modelDataName: viewport.name + "_toolbar"
    }
    XsPreference {
        id: __toolbarHiddenItems
        index: globalStoreModel.searchRecursive("/ui/viewport/hidden_toolbar_items", "pathRole")
    }
    property alias toolbarHiddenItems: __toolbarHiddenItems.value

    Repeater {
        model: toolbar_model_data
        Item {
            XsMenuModelItem {
                text: title
                menuItemType: "toggle"
                menuPath: "Toolbar"
                menuItemPosition: index
                menuModelName: btnMenu.menu_model_name
                isChecked: !toolbarHiddenItems.includes(text)
                onActivated: {
                    var t = toolbarHiddenItems
                    if (t.includes(text)) {
                        var v = t.indexOf(text)
                        t.splice(v, 1)
                    } else {
                        t.push(text)
                    }
                    toolbarHiddenItems = t
                }
                panelContext: btnMenu.panelContext
            }
        }
    }

    // XsMenuModelItem {
    //     menuItemType: "divider"
    //     menuItemPosition: 29
    //     menuPath: ""
    //     menuModelName: btnMenu.menu_model_name
    // }

    XsMenuModelItem {
        text: "Snapshot viewer..."
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 29 //30
        menuModelName: btnMenu.menu_model_name
        onActivated: {
            view.doSnapshot()
        }
        panelContext: btnMenu.panelContext
        Component.onCompleted: {
            setMenuPathPosition("UI Elements", 3.0)
            setMenuPathPosition("Toolbar", 4.0)
        }
    }

}
