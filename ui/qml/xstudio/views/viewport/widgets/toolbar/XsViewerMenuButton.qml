// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

XsViewerToolbarButtonBase {

    id: theButton
    isActive: menu_loader.item ? menu_loader.item.visible : false
    showBorder: mouseArea.containsMouse
    property bool menu_visible: false

    MouseArea{

        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: !disabled // from parent item class
        onClicked: {
            if (menu_visible) {
                // see note down the bottom
                return
            }
            if (menu_loader.item == undefined) {
                menu_loader.sourceComponent = menuComponent
            }
            if (menu_loader.item.visible) {
                menu_loader.item.visible = false
            } else {
                menu_visible = true
                menu_loader.item.showMenu(
                    theButton,
                    0,
                    -menu_loader.item.height)
            }
        }

    }

    // This menu works by picking up the 'value' and 'combo_box_options' role
    // data that is exposed via the model that instantiated this XsViewerMenuButton
    // instance


    Loader {
        id: menu_loader
    }

    Component {
        id: menuComponent
        XsMenuMultiChoice {
            id: btnMenu
            visible: false
            property bool is_enabled: true
            onVisibleChanged: {
                if (!visible) {
                    menu_loader.sourceComponent = undefined
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