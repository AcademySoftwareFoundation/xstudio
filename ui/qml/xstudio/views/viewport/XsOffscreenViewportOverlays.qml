import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "./widgets"
import "./hud"

// This Item extends the pure 'Viewport' QQuickItem from the cpp side

Item  {

    //anchors.fill: parent

    width: 1920
    height: 1080
    id: view
    property var name: "BMDecklinkPlugin viewport"
    property var imageBoundariesInViewport
    property var imageResolutions
    property var sessionActorAddr
    property var helpers

    /*****************************************************************
     *
     * The XsOffscreenViewportOverlays is created in a different QML Engine
     * to the rest of the xSTUDIO UI. It therefore needs its own
     * XsSessionData instance, so that info about the session (like
     * current on-screen media properties) are made available to 
     * overlay graphics items
     *
     ****************************************************************/
    XsSessionData {
        id: sessionData
        sessionActorAddr: view.sessionActorAddr
    }

    // Makes these important global items visible for child contexts, namely
    // overlays and HUDs that might need this info
    property alias theSessionData: sessionData.session
    property alias viewedMediaSetProperties: sessionData.viewedMediaSetProperties
    property alias currentPlayhead: sessionData.current_playhead
    property alias callbackTimer: sessionData.callbackTimer
    property alias sessionProperties: sessionData.sessionProperties
    property alias bookmarkModel: sessionData.bookmarkModel
    property alias currentOnScreenMediaData: sessionData.currentOnScreenMediaData
    property alias currentOnScreenMediaSourceData: sessionData.currentOnScreenMediaSourceData
    property alias viewportCurrentMediaContainerIndex: sessionData.viewportCurrentMediaContainerIndex

    property var sessionPath: sessionProperties.values.pathRole
    
    XsViewportHUD {}

    XsViewportOverlays {}

}
