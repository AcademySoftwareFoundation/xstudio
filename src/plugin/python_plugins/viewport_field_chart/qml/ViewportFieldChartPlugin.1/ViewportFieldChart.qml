// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick
import QtQuick.Effects

// These imports are necessary to have access to custom QML types that are
// part of the xSTUDIO UI implementation.
import xStudio 1.0
import xstudio.qml.models 1.0

// Our Overlay is based on an item that simply fills the 
// xSTUDIO viewport. Within this we draw the overlay graphics as required.
Item {

    // imageBoundary is the pixel coordinates of the area of the Viewport that
    // is covered by an image. If 'multiHUD' is true it means that multiple
    // images are on-screen (e.g. Grid compare mode) and the HUD items are 
    // already scaled to track each image. Otherwise the HUD item is scaled to
    // fit the whole of the viewport (so we can tuck HUD info into the corners
    // independently of image pan/zoom). For this plugin we always want to
    // track imageBoundary

    id: control
    width: multiHUD ? parent.width : imageBoundary.width
    height: multiHUD ? parent.height : imageBoundary.height
    x: multiHUD ? 0 : imageBoundary.x
    y: multiHUD ? 0 : imageBoundary.y

    XsModuleData {
        id: field_charts_data
        modelDataName: "fieldchart_image_set"
    }

    XsAttributeValue {
        id: __active_charts
        attributeTitle: "Active Charts"
        model: field_charts_data
    }
    property alias active_charts: __active_charts.value

    XsModuleData {
        id: fieldchart_settings
        modelDataName: "fieldchart_settings"
    }

    XsAttributeValue {
        id: __opacity
        attributeTitle: "Opacity"
        model: fieldchart_settings
    }
    property alias fieldCharOpacity: __opacity.value

    XsAttributeValue {
        id: __colour
        attributeTitle: "Colour"
        model: fieldchart_settings
    }
    property alias fieldCharColour: __colour.value

    Repeater {
        model: active_charts
        Item {

            anchors.fill: parent

            Image {
                id: im
                anchors.fill: parent
                sourceSize: Qt.size(2560, 1440)
                source: active_charts[index]
                mipmap: true
            }

            MultiEffect {
                colorization: 1
                colorizationColor: Qt.rgba(fieldCharColour.r, fieldCharColour.g, fieldCharColour.b, 1.0)
                anchors.fill: parent
                source: im
                brightness: 1
            }

            opacity: fieldCharOpacity
        }
    }

    opacity: fieldCharOpacity
}