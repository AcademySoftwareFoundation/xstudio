// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14
import QtQuick.Layouts 1.4

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

ApplicationWindow {

    id: appWindow
    visible: true
    color: "#00000000"
    title: fileName
    objectName: "xstudio_popout_window"

    // this gives us access to the 'role' data of the entry in the session model
    // for the current on-screen media SOURCE
    property var fileName: {
        if (currentOnScreenMediaSourceData.values.pathRole != undefined) {
            return helpers.fileFromURL(currentOnScreenMediaSourceData.values.pathRole)
        }
        return "xSTUDIO"
    }

    property var window_name: "popout_window"

    XsPanelsLayoutModel {

        id: ui_layouts_model
        property var root_index: ui_layouts_model.index(-1, -1)
        onJsonChanged: {
            positionWindow()
        }
        Component.onCompleted: {
            positionWindow()
        }
        function positionWindow() {
            root_index = ui_layouts_model.searchRecursive(window_name, "window_name")
            if (root_index.valid) {
                appWindow.x = ui_layouts_model.get(ui_layouts_model.root_index, "position_x")
                appWindow.y = ui_layouts_model.get(ui_layouts_model.root_index, "position_y")
                appWindow.width = ui_layouts_model.get(ui_layouts_model.root_index, "width")
                appWindow.height = ui_layouts_model.get(ui_layouts_model.root_index, "height")
                appWindow.user_data = ui_layouts_model.get(ui_layouts_model.root_index, "user_data")
            }
        }
    }

    property var user_data
    onUser_dataChanged: {
        if (ui_layouts_model.root_index.valid)
            ui_layouts_model.set(ui_layouts_model.root_index, user_data, "user_data")
    }

    onXChanged: {
        if (ui_layouts_model.root_index.valid)
            ui_layouts_model.set(ui_layouts_model.root_index, x, "position_x")
    }

    onYChanged: {
        if (ui_layouts_model.root_index.valid)
            ui_layouts_model.set(ui_layouts_model.root_index, y, "position_y")
    }

    onWidthChanged: {
        if (ui_layouts_model.root_index.valid)
            ui_layouts_model.set(ui_layouts_model.root_index, width, "width")
    }

    onHeightChanged: {
        if (ui_layouts_model.root_index.valid)
            ui_layouts_model.set(ui_layouts_model.root_index, height, "height")
    }

    // if we want to show a pop-up in a particular place, we need to work out
    // the pop-up coordinates relative to it's parent. This can be a bit awkward
    // if we are doing it relative to some button that is not the parent of
    // the pop-up. This convenience function makes that easy ... it also makes
    // sure menus don't get placed so they go outside the application window
    function repositionPopupMenu(menu, refItem, refX, refY, altRightEdge) {

        var global_pos = refItem.mapToItem(
            appWindow.contentItem,
            refX,
            refY
            )

        if (altRightEdge !== undefined) {

            // altRightEdge allows us to position a pop-up to the left of
            // its parent menu, if the pop-up would otherwise go outside the
            // right edge of the window
            var global_pos_alt = refItem.mapToItem(
                appWindow.contentItem,
                altRightEdge,
                refY
                )

            if ((global_pos.x+menu.width) > appWindow.width) {
                global_pos.x = global_pos_alt.x-menu.width
            }
        }
        global_pos.x = Math.min(appWindow.contentItem.width-menu.width, global_pos.x)
        global_pos.y = Math.min(appWindow.contentItem.height-menu.height, global_pos.y)

        var parent_pos = appWindow.contentItem.mapToItem(
            menu.parent,
            global_pos
            )

        menu.x = parent_pos.x
        menu.y = parent_pos.y
        menu.visible = true

    }

    function checkMenuYPosition(refItem, h, refX, refY) {
        var ypos = refItem.mapToItem(
            appWindow.contentItem,
            refX,
            refY
            ).y
        return Math.max((ypos+h)-appWindow.contentItem.height, 0)
    }

    function maxMenuHeight(origMenuHeight) {
        // limit the maximum menu size to 80% of the height of the window
        // itself as long as the menu is less than 66% of the window height.
        // This meeans that you don't get a menu that is shortened by 2 pixels
        if (origMenuHeight > appWindow.contentItem.height*0.8) {
            return appWindow.contentItem.height*0.6
        }
        return origMenuHeight
    }

    property var preFullScreenVis: [x, y, width, height]
    property var qmlWindowRef: Window  // so javascript can reference Window enums.

    property bool fullscreen: false

    onFullscreenChanged : {
        if (fullscreen && visibility !== Window.FullScreen) {
            preFullScreenVis = [x, y, width, height]
            showFullScreen();
        } else if (!fullscreen) {
            visibility = qmlWindowRef.Windowed
            x = preFullScreenVis[0]
            y = preFullScreenVis[1]
            width = preFullScreenVis[2]
            height = preFullScreenVis[3]
        }
    }

    /*****************************************************************
     *
     * This captures any keyboard input and forwards to the last viewport
     * to become visible or the last viewport which the mouse has entered.
     * Hotkeys are all handled by the viewport
     *
     ****************************************************************/
    XsHotkeyArea {
        anchors.fill: parent
        context: "popoutWindow"
        focus: true
    }

    XsHotkey {
        id: fullscreen_hotkey
        sequence: "Ctrl+F"
        name: "Full Screen"
        description: "Makes the window go fullscreen"
        onActivated: {
            appWindow.fullscreen = !appWindow.fullscreen
        }
    }

    property bool popoutIsOpen: false
    property bool isPopoutViewer: true
    property bool isQuickview: false

    XsViewportPanel {
        anchors.fill: parent
    }

    function set_menu_bar_visibility(menu_bar_visible) {
        // dummy function needed by XsViewportPanel. No menu bar in pop-out window
    }

}

