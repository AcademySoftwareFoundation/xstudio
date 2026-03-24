// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0

XsPopupMenu {

    id: removeMenu
    visible: false
    menu_model_name: "playlist_remove_button_menu"

    // property idenfies the 'panel' that is the anticedent of this
    // menu instance. As this menu is instanced multiple times in the
    // xstudio interface we use this context property to ensure our
    // 'onActivated' callback/signal is only triggered in the corresponding
    // XsMenuModelItem instance.
    property var panelContext: helpers.contextPanel(removeMenu)

    XsMenuModelItem {
        text: "Remove Selected"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: removeMenu.menu_model_name
        onActivated: doRemove()
        panelContext: removeMenu.panelContext
    }
}
