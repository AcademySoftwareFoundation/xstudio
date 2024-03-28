// SPDX-License-Identifier: Apache-2.0
import xStudio 1.0

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtGraphicalEffects 1.12
import QtQuick.Shapes 1.12
import Qt.labs.platform 1.1
import Qt.labs.settings 1.0
import QtQml.Models 2.14
import QtQml 2.14

import QuickFuture 1.0
import QuickPromise 1.0

//------------------------------------------------------------------------------
// BEGIN COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------
import xstudio.qml.viewport 1.0
import xstudio.qml.session 1.0
//import xstudio.qml.playlist 1.0
import xstudio.qml.semver 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.helpers 1.0

Rectangle {

    anchors.fill: parent
    color: "#00000000"
    border.width: 3

    id: sessionWidget

    property real borderWidth: XsStyle.outerBorderWidth

    property alias playerWidget: playerWidget
    property bool is_presentation_mode: true
    property int minimumWidth: 100
    property int minimumHeight: (minimumWidth*9)/16
    property var internal_drag_drop_source_uuid
    property var internal_drag_drop_cursor_position
    property bool is_main_window: false

    property alias is_main_window: sessionWidget.is_main_window

    property string window_name

    XsShortcuts {
        anchors.fill: parent
        id: shortcuts
        context: viewport.name
        //enabled: viewport.enableShortcuts
    }

    // create a binding to prefs where the current layout for this session
    // widget is kept

    XsModelNestedPropertyMap {
        id: prefs
        index: app_window.globalStoreModel.search_recursive("/ui/qml/" + window_name + "_settings", "pathRole")
        property alias properties: prefs.values
    }

    XsDragDropIndicator {
        id: drag_drop_indicator
    }

    function toggleNotes(show=undefined) {
        // To do - fix this madness!
        app_window.sessionWidget.playerWidget.viewportTitleBar.mediaToolsTray.toggleNotesDialog(show)
    }
    function toggleDrawDialog(show=undefined) {
        // To do - fix this madness!
        app_window.sessionWidget.playerWidget.viewportTitleBar.mediaToolsTray.toggleDrawDialog(show)
    }

    XsPlayerWidget {
        id: playerWidget
        visible: true
        parent_win: app_window
        playlist_panel: null
        anchors.fill: parent
        preferencePath: "/ui/qml/" + window_name + "_viewer"
        is_presentation_mode: true
        is_main_window: sessionWidget.is_main_window
    }

    property var viewport: playerWidget.viewport
    Keys.forwardTo: viewport

    XsStatusBar {
        id: status_bar
        opacity: 0
        visible: false
    }

    XsClosedownAnimation {
        id: closedown_anim
    }

    function start_closedown_anim() {
        return closedown_anim.do_anim(sessionWidget);
    }

    // When the viewerLayoutsMenu is visible, a click outside
    // of it should hide it, to do this we need a mouse area
    // underneath it that captures all mouse events while it
    // is visisble
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        hoverEnabled: true
        id: override_mouse_area
        enabled: false
        z: -10000
        onClicked: {
            hideLayoutsMenu()
        }
    }

    XsViewerLayoutsMenu {
        x: sessionWidget.width - x_offset
        y: y_offset
        property int x_offset: 0
        property int y_offset: 0
        id: viewerLayoutsMenu
        opacity: 0
        z: 10000
    }

    function showLayoutsMenu(global_coord) {
        var coord = mapFromGlobal(global_coord.x, global_coord.y)
        viewerLayoutsMenu.x_offset = sessionWidget.width - (coord.x - viewerLayoutsMenu.width)
        viewerLayoutsMenu.y_offset = coord.y
        viewerLayoutsMenu.opacity = 1
        override_mouse_area.enabled = true
        override_mouse_area.z = 9000
    }

    function hideLayoutsMenu() {
        viewerLayoutsMenu.opacity = 0
        override_mouse_area.enabled = true
        override_mouse_area.z = -10000
    }

}
