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
    title: "xSTUDIO QuickView: " + mediaImageSource.fileName
    minimumHeight: 320
    minimumWidth: 480
    id: light_player
    property var preFullScreenVis: [app_window.x, app_window.y, app_window.width, app_window.height]
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

    onClosing: {
        destroy()
        app_window.closingQuickviewWindow(Qt.point(x, y),  Qt.size(width, height))
    }

    Component.onCompleted: {
        requestActivate()
        raise()
    }

    property var sessionModel

    // This thing monitors the 'mediaUuid' which is a property of the playhead
    // and tells us the uuid of the media that is on-screen. When mediaUuid
    // changes, we find the model data for the media in the sessionModel. We
    // then use this to get the 'imageActorUuidRole' which gives us the uuid
    // of the media source (video). We then search the session again to find 
    // the media source (video) model data, and update mediaImageSource to
    // track it.

    XsTimer {
        id: m_timer
    }

    XsModelPropertyMap {
        id: currentMediaItem
        index: sessionModel.index(-1,-1)
        property var screenMediaUuid: mediaUuid
        property var imageSource: values ? values.imageActorUuidRole ? values.imageActorUuidRole : undefined : undefined

        onScreenMediaUuidChanged: {
            index = sessionModel.search_recursive(mediaUuid, "actorUuidRole")
        }

        function setMediaImageSource() {
            let mind = sessionModel.search_recursive(imageSource, "actorUuidRole")
            if(mind.valid && mediaImageSource.index != mind) {
                mediaImageSource.index = mind
                return true
            }
            return false
        }

        onImageSourceChanged: {

            if(!setMediaImageSource()) {
                // This is a bit ugly - the session model is a bit behind us,
                // and it hasn't yet been updated with the media item that we're
                // interested in (where actorUuidRole == imageSource).
                // Therefore we repeat the search 100ms later
                m_timer.setTimeout(currentMediaItem.setMediaImageSource, 100)
            }

        }

    }

    // current MT_IMAGE media source
    property alias mediaImageSource: mediaImageSource
    XsModelPropertyMap {
        id: mediaImageSource
        index: sessionModel.index(-1,-1)
        property var fileName: {
            let result = ""
            if(index.valid && values.pathRole != undefined) {
                result = helpers.fileFromURL(values.pathRole)
            }
            return result
        }

    }
    
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

    XsLightPlayerWidget {
        id: sessionWidget
        anchors.fill: parent
        focus: true
    }

    property alias playerWidget: sessionWidget
    property var viewport: sessionWidget.viewport
    property var mediaUuid: viewport.playhead ? viewport.playhead.mediaUuid : undefined
    property alias sessionWidget: sessionWidget

}
