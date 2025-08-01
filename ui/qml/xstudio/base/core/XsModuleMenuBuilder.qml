// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

import xstudio.qml.module 1.0

import xStudio 1.0

Item {

    id: parent_item
    property var parent_menu
    property var root_menu_name
    property var insert_after
    property int insert_after_index: -1

    property var ct: parent_menu.count
    onCtChanged: set_insert_index()
    property var empty: module_menu_shim.empty

    onInsert_afterChanged: set_insert_index()
    onParent_menuChanged: set_insert_index()

    function set_insert_index() {
        if (parent_menu && insert_after) {
            for (var idx = 0; idx < parent_menu.count; idx++) {
                if (itemAt(idx) == insert_after) {
                    insert_after_index = idx+1;
                    break;
                }
            }
        }
    }

    XsModuleMenu {
        id: module_menu_shim
        root_menu_name: parent_item.root_menu_name
    }

    Instantiator {

        model: module_menu_shim

        delegate: XsModuleMenuItem {
        }
        onObjectAdded: {
            if (insert_after_index != -1) {                
                parent_menu.insertItem(insert_after_index, object)
            } else {
                parent_menu.addItem(object)
            }
        }
        onObjectRemoved: parent_menu.removeItem(object)

    }

    Instantiator {

        model: module_menu_shim.num_submenus

        delegate: XsModuleSubMenu { 
            root_menu_name : module_menu_shim.submenu_names[index]
        }

        // The trick is on those two lines
        onObjectAdded: {
            if (insert_after_index != -1) {                
                parent_menu.insertMenu(insert_after_index, object)
            } else {
                parent_menu.addMenu(object)
            }
        }
        onObjectRemoved: {
            parent_menu.removeMenu(object)
        }
    }
}