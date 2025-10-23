import QtQuick
import QtQuick.Layouts

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
    property var playheadUuid
    property var imageResolutions
    property var sessionActorAddr
    property bool isQuickview: false

    // These dummy signals are present on the regular on-screen viewport and 
    // some overlays that have mouse interaction (e.g. GradingTool) establish
    // connections to them. To prevent errors showing up in the log when using
    // the off-screen viewport we declare dummy signals here to match the 
    // signals that the 'real' viewport has.
    signal mouseRelease(var buttons)
    signal mousePress(var position, var buttons, var modifiers)
    signal mouseDoubleClick(var position, var buttons, var modifiers)
    signal mousePositionChanged(var position, var buttons, var modifiers)

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
    
    XsPlayhead {
        id: viewportPlayhead
        uuid: playheadUuid
    }

    property var hud_plugins_display_data

    property alias viewportPlayhead: viewportPlayhead

    // the property imageBoundariesInViewport updates every time the viewport
    // geometry changes (pan/zoom or image size). We don't want the repeater 
    // below to rebuild the XsViewportHUD items on every update to 
    // imageBoundariesInViewport, so by maintaining a ListModel the Repeater
    // will not full rebuild but just update the model data values that
    // effect the XsViewportHUD instances (the image boundaries per image on
    // screen)
    ListModel{
        id: image_boundaries
    }

    onImageBoundariesInViewportChanged: {
        var n = imageBoundariesInViewport.length
        for (var i = 0; i < n; ++i) {
            if (i < image_boundaries.count) {
                image_boundaries.get(i).imageBoundary = imageBoundariesInViewport[i];
                image_boundaries.get(i).multiHUD = n > 1;
            } else {
                image_boundaries.append({"imageBoundary": imageBoundariesInViewport[i], "multiHUD": n > 1})
            }
        }
        if (image_boundaries.count > n) {
            image_boundaries.remove(n, image_boundaries.count - n)
        }
    }
    // this one lays out the HUD graphics coming from HUD plugins and
    // also general overlay graphics like Mask
    Repeater {
        model: image_boundaries
        XsViewportHUD {
            imageIndex: index
        }
    }

    XsViewportOverlays {}

}
