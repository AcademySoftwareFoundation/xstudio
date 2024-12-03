// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

import "../functions/"


XsPopupMenu {

    id: removeMenu
    visible: false
    menu_model_name: "medialist_remove_button_menu"

    // property idenfies the 'panel' that is the anticedent of this
    // menu instance. As this menu is instanced multiple times in the
    // xstudio interface we use this context property to ensure our
    // 'onActivated' callback/signal is only triggered in the corresponding
    // XsMenuModelItem instance.
    property var panelContext: helpers.contextPanel(removeMenu)

    XsMenuModelItem {
        text: "Remove Selected Media"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: removeMenu.menu_model_name
        onActivated: deleteSelected(true)
        panelContext: removeMenu.panelContext
    }

    XsMenuModelItem {
        text: "Remove Offline Media"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: removeMenu.menu_model_name
        onActivated: deleteOffline(true)
        panelContext: removeMenu.panelContext
    }
}
