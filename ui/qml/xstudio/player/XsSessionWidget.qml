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
import xstudio.qml.semver 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.helpers 1.0

Rectangle {

    anchors.fill: parent
    color: "#00000000"
    border.width: 3

    id: sessionWidget

    property real borderWidth: XsStyle.outerBorderWidth

    property var sessionMenu: menu_row.menuBar
    property var mediaMenu1: media_list.mediaMenu

    property alias playerWidget: playerWidget
    property alias media_list: media_list
    property alias playlist_panel: playlist_panel
    property alias is_main_window: sessionWidget.is_main_window
    property alias viewport: playerWidget.viewport

    property var status_bar_visible: playerWidget.status_bar_visible
    property var layout_bar_visible: playerWidget.layout_bar_visible
    property var menu_bar_visible: playerWidget.menu_bar_visible
    property var viewport_title_bar_visible: playerWidget.viewport_title_bar_visible
    property bool is_presentation_mode: presentation_layout.visible
    property int minimumWidth: menuBar.implicitWidth
    property int minimumHeight: (minimumWidth*9)/16
    property var internal_drag_drop_source_uuid
    property var internal_drag_drop_cursor_position
    property bool is_main_window: true

    property string window_name

    XsShortcuts {
        anchors.fill: parent
        id: shortcuts
        context: viewport.name
    }

    // create a binding to prefs where the current layout for this session
    // widget is kept
    XsModelNestedPropertyMap {
        id: prefs
        index: app_window.globalStoreModel.search_recursive("/ui/qml/" + window_name + "_settings", "pathRole")
        property alias properties: prefs.values

    }

    property string layout_name: prefs.values.layout_name !== undefined ? prefs.values.layout_name : ""
    property string previous_layout: prefs.values.layout_name !== undefined ? prefs.values.layout_name : ""

    PropertyAnimation {
        id: border_animator
        target: sessionWidget
        property: "borderWidth"
        running: false
        duration: XsStyle.presentationModeAnimLength
    }

    XsDragDropIndicator {
        id: drag_drop_indicator
    }

    function dragging_items_from_media_list(source_uuid, parent_playlist_uuid, drag_drop_items, mousePos) {
        playlist_panel.dragging_items_from_media_list(source_uuid, parent_playlist_uuid, drag_drop_items, mousePos)
        var local_pos = mapFromGlobal(mousePos.x, mousePos.y)
        drag_drop_indicator.x = local_pos.x
        drag_drop_indicator.y = local_pos.y
        drag_drop_indicator.drag_drop_items = drag_drop_items
    }

    function dropping_items_from_media_list(source_uuid, source_playlist, drag_drop_items, mousePos) {
        drag_drop_indicator.drag_drop_items = []
        playlist_panel.dropping_items_from_media_list(source_uuid, source_playlist, drag_drop_items, mousePos)
    }

    // When the viewerLayoutsMenu is visible, a click outside
    // of it should hide it, to do this we need a mouse area
    // underneath it that captures all mouse events while it
    // is visisble
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        hoverEnabled: false
        id: override_mouse_area
        enabled: false
        z: -10000
        onClicked: {
            if (hider) hider()
            enabled = false
            z = -10000
        }
        function activate(hider_func) {
            enabled = true
            z = 9000
            hider = hider_func
        }
        function deactivate() {
            if (hider) hider()
            enabled = false
            z = -10000
        }
        property var hider
    }

    RowLayout {

        id: menu_row
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0
        property alias menuBar: menuBar

        XsMenuBar {
            id: menuBar

            Layout.fillWidth: true
            playerWidget: sessionWidget.playerWidget
            Behavior on opacity {
                NumberAnimation { duration: playerWidget.doTrayAnim? XsStyle.presentationModeAnimLength : 0 }
            }
            opacity: menu_bar_visible && (!is_presentation_mode || playerWidget.controlsVisible)
        }

        Rectangle {
            id: viewerLayoutsButton
            width: XsStyle.tabBarHeight
            Layout.fillHeight: true
            color: viewerLayoutsMouseArea.containsMouse ? preferences.accent_color : XsStyle.mainBackground

            XsColoredImage {
                anchors.fill:parent
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                myIcon.width: XsStyle.tabBarHeight / 2
                myIcon.height: XsStyle.tabBarHeight / 2
                source: "qrc:///feather_icons/menu.svg"
                MouseArea {
                    id: viewerLayoutsMouseArea
                    visible: playerWidget.controlsVisible
                    cursorShape: Qt.PointingHandCursor
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onClicked: {
                        var x = width/3
                        var y = height/3
                        sessionWidget.showLayoutsMenu(mapToGlobal(x, y))
                    }
                }
            }
        }

    }

    XsLayoutsBar {
        id: layouts_bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: menu_row.bottom
        height: 30*opacity
        opacity: layout_bar_visible
        visible: opacity != 0.0
        Behavior on opacity {
            NumberAnimation {duration: 200}
        }

    }

    XsStatusBar {
        id: status_bar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: status_bar_visible
        visible: opacity != 0.0
        Behavior on opacity {
            NumberAnimation {duration: 200}
        }

    }

    property alias status_bar: status_bar

    Repeater {
        id: repeater
        model: studio.dataSources

        XsPluginWidget {
            qmlWidgetString: modelData.qmlWidgetString
            qmlMenuString: modelData.qmlMenuString
            qmlName: modelData.qmlName
            backendId: modelData.backendId
        }
    }

    Rectangle {

        anchors.top: layouts_bar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: status_bar.top
        color: "#00000000"

        // These are the needed for the thin borders
        // ('gutters') surrounding the main layout of panels
        Item {
            anchors.fill: parent
            visible: !is_presentation_mode
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: borderWidth
                color: XsStyle.mainBackground
            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: borderWidth
                color: XsStyle.mainBackground
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.right: parent.right
                height: borderWidth
                color: XsStyle.mainBackground
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                height: borderWidth
                color: XsStyle.mainBackground
            }
        }

        XsPaneContainer {

            id: session_panel_container

            //right_divider: horiz_divider

            header_component: "qrc:/panels/playlist/XsSessionPanelHeader.qml"

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                XsPlaylistsPanelNew {
                    id: playlist_panel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            onWidthChanged: {
                if (width <= 0) {
                    visible = false
                } else {
                    visible = true
                }
            }
        }

        XsPaneContainer {

            id: player_container

            title: "Viewer"
            header_visible: false//playerWidget.controlsVisible

            has_borders: !is_presentation_mode

            XsPlayerWidget {
                id: playerWidget
                visible: true
                parent_win: app_window
                playlist_panel: playlist_panel
                Layout.fillWidth: true
                Layout.fillHeight: true
                preferencePath: "/ui/qml/" + window_name + "_viewer"
                is_presentation_mode: sessionWidget.is_presentation_mode
                is_main_window: sessionWidget.is_main_window
            }

        }

        XsPaneContainer {

            id: timline_container
            title: "Media"
            visible: height > 0

            //bottom_divider: vert_divider2

            header_component: "qrc:/panels/timeline/XsTimelinePanelHeader.qml"

            XsTimelinePanel {
                id: timeline
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        XsPaneContainer {

            id: media_list_container

            title: "Media"
            visible: height > 0

            //top_divider: vert_divider2

            header_component: "qrc:/bars/XsMediaListPanelHeader.qml"

            // ColumnLayout {

            //     Layout.fillWidth: true
            //     Layout.fillHeight: true
            //     spacing: 0

                XsMediaPanelListView {
                    id: media_list
                    // header: header
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

            // }
        }

        Xs3PaneLayout {
            id: review_layout
            anchors.fill: parent
            anchors.margins: borderWidth
            active: true
            parent_window_name: window_name
            layout_name: "review_layout"
            child_widget_A: session_panel_container
            child_widget_B: media_list_container
            child_widget_C: player_container
        }

        Xs2PaneLayout {
            id: browse_layout
            anchors.fill: parent
            anchors.margins: borderWidth
            layout_name: "browse_layout"
            parent_window_name: window_name
            child_widget_A: session_panel_container
            child_widget_B: media_list_container
        }

        Xs4PaneLayout {
            id: edit_layout
            anchors.fill: parent
            anchors.margins: borderWidth
            parent_window_name: window_name
            layout_name: "edit_layout"
            child_widget_A: session_panel_container
            child_widget_B: media_list_container
            child_widget_C: player_container
            child_widget_D: timline_container
        }

        XsPresentationLayout {
            id: presentation_layout
            anchors.fill: parent
            anchors.margins: 0
            parent_window_name: window_name
            child_widget_A: player_container
        }

    }

    property alias presentation_layout: presentation_layout
    property alias edit_layout: edit_layout
    property alias browse_layout: browse_layout
    property alias review_layout: review_layout
    property var the_layout: review_layout
    property var all_panels: [session_panel_container, player_container, media_list_container, timline_container]

    onLayout_nameChanged: {
        previous_layout = prefs.values.layout_name

        if (layout_name == "browse_layout") {
            the_layout.active = false
            the_layout = browse_layout
            switch_layout()
        } else if (layout_name == "edit_layout") {
            the_layout.active = false
            the_layout = edit_layout
            switch_layout()
        } else if (layout_name == "review_layout") {
            the_layout.active = false
            the_layout = review_layout
            switch_layout()
        } else if (layout_name == "presentation_layout") {
            // previousControlVisible = playerWidget.controlsVisible
            // playerWidget.controlsVisible = false
            the_layout.active = false
            the_layout = presentation_layout
            switch_layout()
        }
        prefs.values.layout_name = layout_name
    }

    function toggleNotes(show=undefined) {
        // To do - fix this madness!
        app_window.sessionWidget.playerWidget.viewportTitleBar.mediaToolsTray.toggleNotesDialog(show)
    }
    function toggleDrawDialog(show=undefined) {
        // To do - fix this madness!
        app_window.sessionWidget.playerWidget.viewportTitleBar.mediaToolsTray.toggleDrawDialog(show)
    }

    function switch_layout() {


        for (var idx = 0; idx < all_panels.length; idx++) {
            if (!the_layout.contains(all_panels[idx])) {
                all_panels[idx].visible = false
            } else {
                all_panels[idx].visible = true
            }
        }
        the_layout.active = true
    }

    function togglePresentationLayout() {
        if (layout_name != "presentation_layout") {
            layout_name = "presentation_layout"
        } else if (previous_layout != "" && sessionWidget.previous_layout != "presentation_layout") {
            layout_name = previous_layout
        } else {
            layout_name = "review_layout"
        }
    }

    function toggleMediaPane() {

        the_layout.toggle_widget_vis(media_list_container)

    }

    function toggleSessionPane() {

        the_layout.toggle_widget_vis(session_panel_container)

    }

    function toggleViewerPane() {

        the_layout.toggle_widget_vis(player_container)

    }

    XsSettingsDialog {
        id: settings_dialog
    }

    function toggleSettingsDialog() {
        if (settings_dialog.visible) {
            settings_dialog.close()
        } else {
            settings_dialog.show()
        }
    }

    function toggleTimelinePane() {

        the_layout.toggle_widget_vis(timline_container)

    }

    property var media_pane_visible: media_list_container.visible
    property var session_pane_visible: session_panel_container.visible
    property var player_pane_visible: player_container.visible
    property var timeline_pane_visible: timline_container.visible
    property var settings_dialog_visible: settings_dialog ? settings_dialog.visible : false
    property var settings_dialog: undefined

    XsClosedownAnimation {
        id: closedown_anim
    }

    function start_closedown_anim() {
        return closedown_anim.do_anim(sessionWidget);
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
        override_mouse_area.activate(hideLayoutsMenu)
    }

    function hideLayoutsMenu() {
        viewerLayoutsMenu.opacity = 0
    }

    function activateWidgetHider(window_to_hide) {
        override_mouse_area.activate(window_to_hide)
    }

    function deactivateWidgetHider() {
        override_mouse_area.deactivate()
    }

}
