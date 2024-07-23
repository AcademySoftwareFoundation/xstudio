// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtGraphicalEffects 1.12
import QtQuick.Shapes 1.12
import Qt.labs.platform 1.1
import Qt.labs.settings 1.0
import QtQml.Models 2.14
import QtQml 2.14


//------------------------------------------------------------------------------
// BEGIN COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------
import xstudio.qml.viewport 1.0
import xstudio.qml.semver 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.uuid 1.0
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

//------------------------------------------------------------------------------
// END COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------


import xStudio 1.0

Rectangle {

    id: playerWidget
    visible: true
    color: "#00000000"

    property var global_store

    property var qmlWindowRef: Window  // so javascript can reference Window enums.
    property bool controlsVisible: true
    property bool doTrayAnim: true
    property var parent_win
    property alias player_prefs: player_prefs
    property bool is_full_screen: { parent_win ? parent_win.visibility == Window.FullScreen : false }
    property var playlist_panel
    property string preferencePath: ""
    property bool is_presentation_mode: false

    // quick fix for v1.0.1
    property var vp_visible: player_prefs.properties.viewportTitleBarVisible !== undefined ? player_prefs.properties.viewportTitleBarVisible : true

    property bool compact_mode: is_main_window ? player_prefs.properties.compactMode : false
    property bool media_info_bar_visible: controlsVisible ? compact_mode ? false : player_prefs.properties.mediaInfoBarVisible : 0
    property bool tool_bar_visible: controlsVisible ? compact_mode ? false : player_prefs.properties.toolBarVisible : 0
    property bool transport_controls_visible: controlsVisible ? compact_mode ? true : player_prefs.properties.transportControlsVisible : 0
    property bool status_bar_visible: (controlsVisible || !is_presentation_mode) ? compact_mode ? false : player_prefs.properties.statusBarVisible : 0
    //property bool layout_bar_visible: (controlsVisible || !is_presentation_mode) ? (compact_mode ? true : player_prefs.properties.layoutBarVisible) : 0
    property bool layout_bar_visible: (controlsVisible || !is_presentation_mode) ? (compact_mode ? true : player_prefs.properties.layoutBarVisible) : 0
    property bool viewport_title_bar_visible: (controlsVisible || !is_presentation_mode) ? compact_mode ? true : vp_visible : 0
    property bool menu_bar_visible: (controlsVisible || !is_presentation_mode) ? player_prefs.properties.menuBarVisible : 0

    property bool is_main_window

    XsModelNestedPropertyMap {
        id: player_prefs
        index: app_window.globalStoreModel.search_recursive(playerWidget.preferencePath, "pathRole")
        property alias properties: player_prefs.values
    }

    function toggleFullscreen() {
        parent_win.toggleFullscreen()
    }

    function normalScreen() {
        parent_win.normalScreen()
    }

    function toggleControlsVisible() {

        if (compact_mode) {
            player_prefs.properties.compactMode = false
        }
        // For now, we retain the animation effect when hiding/revealing controls en masse
        // doTrayAnim = false
        controlsVisible = !controlsVisible
        // doTrayAnim = true
    }

    function toggleMediaInfoBarVisible() {
        forceCompactModeOff()
        player_prefs.properties.mediaInfoBarVisible = !player_prefs.properties.mediaInfoBarVisible
    }

    function toggleToolBarVisible() {
        forceCompactModeOff()
        player_prefs.properties.toolBarVisible = !player_prefs.properties.toolBarVisible
    }

    function toggleTransportControlsVisible() {
        forceCompactModeOff()
        player_prefs.properties.transportControlsVisible = !player_prefs.properties.transportControlsVisible
    }

    function toggleStatusBarVisible() {
        forceCompactModeOff()
        player_prefs.properties.statusBarVisible = !player_prefs.properties.statusBarVisible
    }

    function toggleLayoutBarVisible() {
        forceCompactModeOff()
        player_prefs.properties.layoutBarVisible = !player_prefs.properties.layoutBarVisible
    }

    function toggleViewportTitleBarVisible() {
        forceCompactModeOff()
        player_prefs.properties.viewportTitleBarVisible = !player_prefs.properties.viewportTitleBarVisible
    }

    function toggleMenuBarVisible() {
        player_prefs.properties.menuBarVisible = !player_prefs.properties.menuBarVisible
    }

    function compactModeOn() {
        player_prefs.properties.compactMode = true
    }

    function compactModeOff() {
        player_prefs.properties.compactMode = false
    }

    function forceCompactModeOff() {
        if (compact_mode) {
            player_prefs.properties.toolBarVisible = false
            player_prefs.properties.statusBarVisible = false
            player_prefs.properties.layoutBarVisible = true
            player_prefs.properties.viewportTitleBarVisible = true
            player_prefs.properties.transportControlsVisible = true
            player_prefs.properties.mediaInfoBarVisible = false
            player_prefs.properties.compactMode = false
        }
    }

    property alias viewport: viewport
    property alias viewportTitleBar: viewportTitleBar
    property var playhead: viewport.playhead

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        RowLayout {

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            /*Rectangle {
                color: XsStyle.mainBackground
                width: 16
                Layout.fillHeight: true
            }*/

            ColumnLayout {

                id: playerWidgetItem
                spacing: 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                // visible: !xstudioWarning.visible
                property alias winWidth: playerWidget.width

                XsViewportTitleBar {
                    id: viewportTitleBar
                    Layout.fillWidth: true
                    opacity: viewport_title_bar_visible
                }

                XsMediaInfoBar {
                    id: mediaInfoBar
                    objectName: "mediaInfoBar"
                    Layout.fillWidth: true
                    opacity: media_info_bar_visible
                }

                XsViewport {
                    id: viewport
                    objectName: "viewport"
                    Layout.fillWidth: true
                    Layout.fillHeight: true            
                }

                XsToolBar {
                    id: toolBar
                    Layout.fillWidth: true
                    opacity: tool_bar_visible
                }

                Rectangle {
                    color: XsStyle.mainBackground
                    height: 8*opacity
                    Layout.fillWidth: true
                    visible: opacity !== 0
                    opacity: transport_controls_visible
                    Behavior on opacity {
                        NumberAnimation { duration: playerWidget.doTrayAnim?200:0 }
                    }
                }

                XsMainControls {
                    id: myMainControls
                    Layout.fillWidth: true
                    opacity: transport_controls_visible
                }

                Rectangle {
                    color: XsStyle.mainBackground
                    height: 8*opacity
                    Layout.fillWidth: true
                    visible: opacity !== 0
                    opacity: transport_controls_visible
                    Behavior on opacity {
                        NumberAnimation { duration: playerWidget.doTrayAnim?200:0 }
                    }
                }


            }

            /*Rectangle {
                color: XsStyle.mainBackground
                width: 16
                Layout.fillHeight: true
            }*/

        }
    }

    property alias toolBar: toolBar

}
