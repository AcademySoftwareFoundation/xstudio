// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14
import QtQuick.Layouts 1.4

import xStudio 1.0
import xstudio.qml.helpers 1.0

XsWindow {

    id: floatingWindow
    title: view_name

    property var name: view_name // 'view_name' provided by model
    property var content_qml: view_qml_source // 'view_qml_source' provided by model
    property var content_item

    // we need this to wipe hotkey_uuid property that might be visible in the
    // context that created the floating window (and will pollute all buttons
    // with an incorrect hotkey in the tooltip!)
    property var hotkey_uuid 

    property var user_data: uiLayoutsModel.retrieveFloatingWindowData(name)
    onUser_dataChanged: {
        uiLayoutsModel.storeFloatingWindowData(name, user_data)
    }

    onClosing: {
        window_is_visible = false
    }

    onWidthChanged: {
        if (content_item) content_item.width = width
        storeSizeAndPosition()
    }

    onHeightChanged: {
        if (content_item) content_item.height = height
        storeSizeAndPosition()
    }

    onXChanged: storeSizeAndPosition()
    onYChanged: storeSizeAndPosition()

    onVisibleChanged: {
        // use last positin/size numbers if we have any
        if (visible && user_data && user_data.hasOwnProperty("window_geometry")) {
            x = user_data.window_geometry.x
            y = user_data.window_geometry.y
            width = user_data.window_geometry.width
            height = user_data.window_geometry.height
        } else if (visible) {
            // for now, hardcode panel size
            x = 200
            y = 200
            width = 1024
            height = 400
        }

        if (content_item) content_item.visible = visible
    }

    function storeSizeAndPosition() {
        if(updateTimer.running) {
            updateTimer.restart()
        } else {
            updateTimer.start()
        }
    }

    XsHotkeyArea {
        anchors.fill: parent
        focus: true
        context: name
        id: hotkey_area
   }

   onActiveFocusItemChanged: {
    if (activeFocusItem == floatingWindow.contentItem) {
        // if a widget like a text-entry box that's part of a combobox has 
        // given up its focus, sometimes the window's root contentItem gets 
        // the focus. If this happens we give the focus back to the hotkey
        // area.
        hotkey_area.forceActiveFocus()
    }
   }

    Timer {
        id: updateTimer
        interval: 1000
        running: false
        repeat: false
        onTriggered: {

            // window gets re-positioned by QML when it is hidden, so don't
            // store if not visible
            if (!floatingWindow.visible) return
            // store window position and size in 'user_data' which gets stored
            // in the uiPanelsModelData model
            var p = {}
            p.x = x
            p.y = y
            p.width = width
            p.height = height
            var v = user_data
            if (typeof v != "object") {
                v = {}
            }
            v["window_geometry"] = p
            user_data = v
        }
    }

    onContent_qmlChanged: {

        if (typeof content_qml != "string") return;

        if (content_qml.endsWith(".qml")) {

            let component = Qt.createComponent(content_qml)

            if (component.status == Component.Ready) {

                if (content_item != undefined) content_item.destroy()
                content_item = component.createObject(
                    hotkey_area,
                    {
                    })

                // identify as an XStudioPanel - this is used when menu items
                // are actioned to identify the 'panel' context of the parent
                // menu
            } else {
                console.log("Error loading panel:", component, component.errorString())
            }

        } else {

            content_item = Qt.createQmlObject(content_qml, hotkey_area)
            // see note above

        }

    }

}

