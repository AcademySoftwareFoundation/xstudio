// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {

    title: qsTr("Viewer")
    id: viewer_context_menu_colour_items
    property bool is_popout_viewport: false

    XsMenuItem {
        mytext: qsTr("Presentation Mode")
        shortcut: "Tab"
        mycheckable: true
        onTriggered: {
            sessionWidget.togglePresentationLayout()
        }
        checked: sessionWidget.presentation_layout ? sessionWidget.presentation_layout.active : false
        visible: sessionWidget.is_main_window
        height: visible ? implicitHeight : 0
    }

    XsMenuItem {
        mytext: playerWidget.controlsVisible ? qsTr("Hide UI") : qsTr("Show UI")
        shortcut: "Ctrl+H"
        onTriggered: {
            playerWidget.toggleControlsVisible()
        }
    }

    XsMenuItem {
        mytext: qsTr("Full Screen")
        shortcut: "Ctrl+F"
        mycheckable: true
        onTriggered: {
            playerWidget.toggleFullscreen()
        }
        checked: parent_win ? parent_win.visibility == Window.FullScreen : false
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Set Loop In-Point")
        shortcut: "I"
        onTriggered: {
            playhead.setLoopStart(playhead.frame)
            playhead.useLoopRange = true
        }
    }
    XsMenuItem {
        mytext: qsTr("Set Loop Out-Point")
        shortcut: "O"
        onTriggered: {
            playhead.setLoopEnd(playhead.frame)
            playhead.useLoopRange = true
        }
    }
    XsMenuItem {
        mytext: qsTr("Enable Loop")
        shortcut: "P"
        mycheckable: true
        onTriggered: {
            playhead.useLoopRange = !playhead.useLoopRange
        }
        checked: playhead ? playhead.useLoopRange : 0
    }

    XsMenuSeparator { id: fit_sep }

    XsModuleMenuBuilder {
        parent_menu: viewer_context_menu_colour_items
        root_menu_name: is_popout_viewport ? "popout_viewport_context_menu_items" : "viewport_context_menu_items"
        insert_after: fit_sep
    }

    // need an invisible separator so that we can stuff the 'channel'
    // menu item that is added at runtime by the OCIO plugin in the
    // place we want here ... later we will have a better
    // API for plugins
    MenuSeparator {
        visible: false
        height: 0
        id: dummy_sep
    }

    XsModuleMenuBuilder {
        parent_menu: viewer_context_menu_colour_items
        root_menu_name: is_popout_viewport ? "popout_viewer_context_menu_colour_channel" : "viewer_context_menu_colour_channel"
        insert_after: dummy_sep
    }

    XsMenuItem {
        mytext: qsTr("Reset Viewer")
        shortcut: "Ctrl+R"
        onTriggered: {
            viewport.sendResetShortcut()
        }
    }

    XsMenuSeparator { id: render_sep}

    XsModuleMenuBuilder {
        parent_menu: viewer_context_menu_colour_items
        root_menu_name: is_popout_viewport ? "popout_viewer_context_menu_colour_items" : "viewer_context_menu_colour_items"
        insert_after: render_sep
    }

    XsMenuSeparator { }

    XsMenuItem {
        mytext: qsTr("Snapshot viewer...")
        onTriggered: app_window.toggleSnapshotDialog()
    }
}


