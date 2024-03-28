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
    property alias player_prefs: player_prefs
    property bool is_full_screen: { light_player ? light_player.visibility == Window.FullScreen : false }
    property var playlist_panel
    property string preferencePath: ""
    property bool is_presentation_mode: false
    property bool is_main_window: true
    // quick fix for v1.0.1
    XsShortcuts {
        anchors.fill: parent
        id: shortcuts
        context: viewport.name
        //enabled: viewport.enableShortcuts
    }

    property bool media_info_bar_visible: true
    property bool tool_bar_visible: true
    property bool transport_controls_visible: true

    XsModelNestedPropertyMap {
        id: player_prefs
        index: app_window.globalStoreModel.search_recursive(playerWidget.preferencePath, "pathRole")
        property alias properties: player_prefs.values
    }

    function toggleFullscreen() {
        light_player.toggleFullscreen()
    }

    function normalScreen() {
        light_player.normalScreen()
    }

    XsStatusBar {
        id: status_bar
        opacity: 0
        visible: false
    }

    property alias viewport: viewport
    property var playhead: viewport.playhead

    Keys.forwardTo: viewport
    focus: true

    function toggleControlsVisible() {

        if (media_info_bar_visible) {
            media_info_bar_visible = false
            tool_bar_visible = false
            transport_controls_visible = false
        } else {
            media_info_bar_visible = true
            tool_bar_visible = true
            transport_controls_visible = true
        }
    }

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
                    isQuickViewer: true
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

    // When certain custom pop-up widgets are visible a click outside
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

    function activateWidgetHider(window_to_hide) {
        override_mouse_area.activate(window_to_hide)
    }

    function deactivateWidgetHider() {
        override_mouse_area.deactivate()
    }


}
