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

    // Note that viewport overlays are instanced by the Viewport QML instance
    // which has the id 'view' and is visible to us here. To learn more
    // about the viewport see files XsViewport.qml and qml_viewport.cpp from
    // the xSTUDIO source code.

    id: control
    width: view.width
    height: view.height

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

    // Note: Viewport has property imageBoundariesInViewport - this is a vector
    // of QRect desribing the coordinates of the image(s) in the viewport.
    property var imBoundaries: view.imageBoundariesInViewport

    Repeater {

        model: imBoundaries
        Item {
            property var imageBox: imBoundaries[index]
            Repeater {
                model: active_charts
                Item {

                    x: imageBox.x
                    y: imageBox.y
                    width: imageBox.width
                    height: imageBox.height

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
        }
    }
}