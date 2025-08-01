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
//import xstudio.qml.playlist 1.0
import xstudio.qml.semver 1.0
import xstudio.qml.cursor_pos_provider 1.0
import xstudio.qml.global_store_model 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.session 1.0

//------------------------------------------------------------------------------
// END COMMENT OUT WHEN WORKING INSIDE Qt Creator
//------------------------------------------------------------------------------

// import "../fonts/Overpass"
// import "../fonts/BitstreamVeraMono"
import xStudio 1.0

ApplicationWindow {

    width: 1280
    height: 820
    color: "#00000000"
    title: Qt.application.name// + " v" + Qt.application.version
    minimumHeight: 320
    minimumWidth: 480
    id: app_window
    // flags: Qt.WindowStaysOnTopHint
    property var preFullScreenVis: [app_window.x, app_window.y, app_window.width, app_window.height]
    property alias playerWidget: sessionWidget.playerWidget
    property int vis_before_hide: -1

    palette.base: XsStyle.controlBackground
    palette.text: XsStyle.hoverColor
    palette.button: XsStyle.controlTitleColor
    palette.highlight: highlightColor
    palette.light: highlightColor
    palette.highlightedText: XsStyle.mainBackground
    palette.brightText: highlightColor
    palette.buttonText: XsStyle.hoverColor
    palette.windowText: XsStyle.hoverColor

    function toggle_visible() {
        if (visibility == 0) {
            visibility = 2
            if (vis_before_hide != -1) {
                visibility = vis_before_hide
            }
        } else {
            vis_before_hide = visibility
            visibility = 2
            visibility = 0
        }
    }

    XsGlobalStoreModel {
        id: globalStoreModel
    }
    property alias globalStoreModel: globalStoreModel

    XsWindowStateSaver
    {
        id: win_state_saver
        windowObj: app_window
        windowName: "second_window"
    }

    property var mediaImageSource

    function toggleFullscreen() {
        if (visibility !== Window.FullScreen) {
            preFullScreenVis = [x, y, width, height]
            showFullScreen();
        } else {
            visibility = qmlWindowRef.Windowed
            x = preFullScreenVis[0]
            y = preFullScreenVis[1]
            width = preFullScreenVis[2]
            height = preFullScreenVis[3]
        }
    }

    function normalScreen() {
        if (visibility == Window.FullScreen) {
            toggleFullscreen()
        }
    }

    XsPopoutViewerWidget {
        anchors.fill: parent
        id: sessionWidget
        visible: true
        window_name: "second_window" // this is important for picking up window settings for the 2nd window
        is_main_window: false
        focus: true
        //
    }
    property alias sessionWidget: sessionWidget
    property var viewport: sessionWidget.viewport

}
