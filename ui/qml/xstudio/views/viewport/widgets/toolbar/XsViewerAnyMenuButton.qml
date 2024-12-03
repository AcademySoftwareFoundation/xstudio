// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

XsViewerToolbarButtonBase {

    property var menuModelName
    isActive: loader.item ? loader.item.visible : false
    showBorder: mouseArea.containsMouse
    id: theButton

    // we use a loader so that pop-up menus are only created when we need
    // to show them.
    property bool menu_visible: false

    MouseArea{

        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onPressed: {
            if (menu_visible) {
                // See note below
                return
            }
            if (!loader.item) {
                loader.sourceComponent = menuComponent
            }
            if (loader.item.visible) {
                loader.item.visible = false
            } else {
                menu_visible = true
                loader.item.showMenu(
                    theButton,
                    0,
                    -loader.item.height)
            }
        }

    }

    Loader {
        id: loader
    }

    function hide_menu() {
        if (loader.item && loader.item.visible) {
            loader.item.visible = false
        }
    }

    // This menu is built from a menu model that is maintained by xSTUDIO's
    // backend. We access the menu model by an id string 'menuModelName' that
    // will be set by the derived type
    Component {
        id: menuComponent
        XsPopupMenu {
            id: btnMenu
            visible: false
            menu_model_name: menuModelName
            onVisibleChanged: {
                if (!visible) {
                    loader.sourceComponent = undefined
                    // awkward - if the user hides the menu with a click in the
                    // mouseArea, 'onPressed' is called *after* the menu is
                    // automatically hidden. So in onPressed we check 'menu_visible'
                    // to stop trying to show the menu again. We need to set
                    // menu_visible to false with a slight delay
                    callbackTimer.setTimeout(function(theButton) { return function() {
                        theButton.menu_visible = false
                        }}(theButton), 100);
                }
            }

        }
    }

}
