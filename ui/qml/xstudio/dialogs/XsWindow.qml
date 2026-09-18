import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

Window {
    minimumWidth: 150
    minimumHeight: 100

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.WindowStaysOnTopHint  | Qt.Dialog | Qt.Tool

    color: XsStyleSheet.panelBgColor

    XsGradientRectangle{
        anchors.fill: parent
    }

    XsFocusRemover {}

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

    // when showing a pop-up menu at some local coordinate we might need to
    // shift it so that it doesn't go outside the window bounds. To do that
    // we need to know the window geometry. This function is called from the
    // XsPopupMenu item, passing in the coordinates relative to a reference
    // item where we ideally want the menu to show up.
    function maxMenuHeight(origMenuHeight) {
        // limit the maximum menu size to 80% of the height of the window
        // itself as long as the menu is less than 66% of the window height.
        // This meeans that you don't get a menu that is shortened by 2 pixels
        if (origMenuHeight > contentItem.height*0.8) {
            return contentItem.height*0.6
        }
        return origMenuHeight
    }


    function repositionPopupMenu(menu, refItem, refX, refY, altRightEdge) {

        var global_pos = refItem.mapToItem(
            contentItem,
            refX,
            refY
            )

        if (altRightEdge !== undefined) {

            // altRightEdge allows us to position a pop-up to the left of
            // its parent menu, if the pop-up would otherwise go outside the
            // right edge of the window
            var global_pos_alt = refItem.mapToItem(
                contentItem,
                altRightEdge,
                refY
                )

            if ((global_pos.x+menu.width) > width) {
                global_pos.x = global_pos_alt.x-menu.width
            }
        }
        global_pos.x = Math.min(contentItem.width-menu.width, global_pos.x)
        global_pos.y = Math.min(contentItem.height-menu.height, global_pos.y)

        var parent_pos = contentItem.mapToItem(
            menu.parent,
            global_pos
            )

        menu.x = parent_pos.x
        menu.y = parent_pos.y
        menu.visible = true

    }

}


