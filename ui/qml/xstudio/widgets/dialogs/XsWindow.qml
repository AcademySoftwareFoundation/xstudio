import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import xStudio 1.0

ApplicationWindow {
    id: window

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.WindowStaysOnTopHint | Qt.Dialog

    color: XsStyleSheet.panelBgColor
 
    // override default palette
    palette.base: XsStyleSheet.panelBgColor
    palette.highlight: XsStyleSheet.accentColor
    palette.text: XsStyleSheet.primaryTextColor
    palette.buttonText: XsStyleSheet.primaryTextColor
    palette.windowText: XsStyleSheet.primaryTextColor
    palette.button: Qt.darker("#414141", 2.4)
    palette.light: "#bb7700"
    palette.highlightedText: Qt.darker("#414141", 2.0)
    palette.brightText: "#bb7700"

    property bool firstTimeShown: true

    onVisibleChanged: {
        if (firstTimeShown) {
            // try and position in the middle of the main session window
            if (typeof appWindow == "object") {
                x = appWindow.x + appWindow.width/2 - width/2
                y = appWindow.y + appWindow.height/2 - height/2
            }
            firstTimeShown = false
        }
    }
    Component.onCompleted: {
        appWindow
    }

    background: XsGradientRectangle{
        id: backgroundDiv
    }

    // when showing a pop-up menu at some local coordinate we might need to
    // shift it so that it doesn't go outside the window bounds. To do that
    // we need to know the window geometry. This function is called from the
    // XsPopupMenu item, passing in the coordinates relative to a reference
    // item where we ideally want the menu to show up.
    function maxMenuHeight(origMenuHeight) {
        // limit the maximum menu size to 80% of the height of the window
        // itself as long as the menu is less than 66% of the window height.
        // This meeans that you don't get a menu that is shortened by 2 pixels
        if (origMenuHeight > window.contentItem.height*0.8) {
            return window.contentItem.height*0.6
        }
        return origMenuHeight
    }


    function repositionPopupMenu(menu, refItem, refX, refY, altRightEdge) {

        var global_pos = refItem.mapToItem(
            window.contentItem,
            refX,
            refY
            )

        if (altRightEdge !== undefined) {

            // altRightEdge allows us to position a pop-up to the left of
            // its parent menu, if the pop-up would otherwise go outside the
            // right edge of the window
            var global_pos_alt = refItem.mapToItem(
                window.contentItem,
                altRightEdge,
                refY
                )

            if ((global_pos.x+menu.width) > window.width) {
                global_pos.x = global_pos_alt.x-menu.width
            }
        }
        global_pos.x = Math.min(window.contentItem.width-menu.width, global_pos.x)
        global_pos.y = Math.min(window.contentItem.height-menu.height, global_pos.y)

        var parent_pos = window.contentItem.mapToItem(
            menu.parent,
            global_pos
            )

        menu.x = parent_pos.x
        menu.y = parent_pos.y
        menu.visible = true

    }

}


