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
    title: "xSTUDIO QuickView - "+fileName
    objectName: "xstudio_quickview_window"

    // this gives us access to the 'role' data of the entry in the session model
    // for the current on-screen media SOURCE
    property var fileName: {
        if (media_source_data.values.pathRole != undefined) {
            return helpers.fileFromURL(media_source_data.values.pathRole)
        }
        return ""
    }

    property var mediaSourceUuid: appWindow.viewport.viewportPlayhead.mediaSourceUuid
    onMediaSourceUuidChanged: {
        set_index(0)
    }
    XsModelPropertyMap {
        id: media_source_data
    }
    function set_index(retry) {
        media_source_data.index = theSessionData.searchRecursive(mediaSourceUuid, "actorUuidRole")
        if (!media_source_data.index.valid && retry < 4) {
            callbackTimer.setTimeout(function(r) { return function() {
                set_index(r+1)
            }}(retry), 200);
        }
    }

    onClosing: {
        // without this, class destruction doesn't seem to happen when the
        // window is closed
        appWindow.destroy()
    }

    // override default palette
    palette.base: XsStyleSheet.panelBgColor
    palette.highlight: XsStyleSheet.accentColor //== "#666666" ? Qt.lighter(XsStyleSheet.accentColor, 1.5) : XsStyleSheet.accentColor
    palette.text: XsStyleSheet.primaryTextColor
    palette.buttonText: XsStyleSheet.primaryTextColor
    palette.windowText: XsStyleSheet.primaryTextColor
    palette.button: Qt.darker("#414141", 2.4)
    palette.light: "#bb7700"
    palette.highlightedText: Qt.darker("#414141", 2.0)
    palette.brightText: "#bb7700"

    FontLoader {id: fontInter; source: "qrc:/fonts/Inter/Inter-Regular.ttf"}

    // child items might expect a user_data property to be in context - normally
    // it comes from the panels layout model so panels can store persistent
    // data, but for quickwindows we don't need to store anything.
    property var user_data: uiLayoutsModel.retrieveFloatingWindowData("quickview_window")
    onUser_dataChanged: {
        uiLayoutsModel.storeFloatingWindowData("quickview_window", user_data)
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

    // this is referenced by some child widgets - normally provided by the
    // layouts framework which isn't employed for quick view windows
    property string panelId: "" + appWindow

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
        context: appWindow.viewport.view.name
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
    property bool isQuickview: true

    XsViewportPanel {
        id: viewportPanel
        anchors.fill: parent
    }
    property alias viewport: viewportPanel

    function set_menu_bar_visibility(menu_bar_visible) {
        // dummy function needed by XsViewportPanel. No menu bar in quickview window
    }

}

