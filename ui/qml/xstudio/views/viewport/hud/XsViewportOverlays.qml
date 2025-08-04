import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    id: hud
    anchors.fill: parent
    clip: true
    
    // here we access attribute data that declares a QML item for drawing
    // overlays into the viewport. Attributes that are added to the
    // 'viewport_overlay_plugins' attribute group and that have 'qml_code'
    // role data are instatiated here

    XsModuleData {
        id: viewport_overlays
        modelDataName: "viewport_overlay_plugins"
    }

    // overlahys are not toggled on/off by HUD on/off
    property bool hud_visible: true

    Repeater {

        id: viewport_overlay_plugins
        anchors.fill: parent
        model: viewport_overlays

        delegate: XsHudItem {
            width: parent.width
            height: parent.height
        }
    }

}
