// SPDX-License-Identifier: Apache-2.0

import QtQuick.Layouts
import QtQuick
import QuickFuture 1.0
import QuickPromise 1.0

// These imports are necessary to have access to custom QML types that are
// part of the xSTUDIO UI implementation.
import xStudio 1.0
import xstudio.qml.models 1.0

// Our Overlay is based on a transparent rectangle that simply fills the
// xSTUDIO view. Within this we draw the overlay graphics as required.
Item {

    // Note that viewport overlays are instanced by the Viewport QML instance
    // which has the id 'view' and is visible to us here. To learn more
    // about the viewport see files Xsview.qml and qml_view.cpp from
    // the xSTUDIO source code.

    id: control

    // imageBoundary & multiHUD are properties of the parent item that instances
    // the HUD items in the xstudio viewport ....
    // imageBoundary is the pixel coordinates of the area of the Viewport that
    // is covered by an image. If 'multiHUD' is true it means that multiple
    // images are on-screen (e.g. Grid compare mode) and the HUD items are 
    // already scaled to track each image. Otherwise the HUD item is scaled to
    // fit the whole of the viewport (so we can tuck HUD info into the corners
    // independently of image pan/zoom). For this plugin we always want to
    // track imageBoundary
    width: multiHUD ? parent.width : imageBoundary.width
    height: multiHUD ? parent.height : imageBoundary.height
    x: multiHUD ? 0 : imageBoundary.x
    y: multiHUD ? 0 : imageBoundary.y

    visible: renderMethod == "QML" && maskEnabled

    // access the attribute group that contains all the settings for the mask.
    // For HUD plugins this is the name of the plugin ("Mask") plus " Settings"
    XsModuleData {
        id: mask_settings
        modelDataName: "Mask Settings"
    }

    /* We can make connections to a single attribute in the group using the
    attribute title */
    XsAttributeValue {
        id: mask_aspect_ratio
        attributeTitle: "Mask Aspect Ratio"
        model: mask_settings
    }
    // make an alias so the aspect is accessible as a regular property to
    // set/get the value
    property alias maskAspectRatio: mask_aspect_ratio.value

    XsAttributeValue {
        id: safety_percent
        attributeTitle: "Safety Percent"
        model: mask_settings
    }
    property alias safetyPercent: safety_percent.value

    property var mask_name: maskAspectRatio.toFixed(2)

    XsAttributeValue {
        id: _mask_enabled
        attributeTitle: "Mask"
        model: mask_settings
    }
    property alias maskEnabled: _mask_enabled.value

    XsAttributeValue {
        id: mask_opacity
        attributeTitle: "Mask Opacity"
        model: mask_settings
    }
    property alias maskOpacity: mask_opacity.value

    XsAttributeValue {
        id: mask_line_opacity
        attributeTitle: "Line Opacity"
        model: mask_settings
    }
    property alias maskLineOpacity: mask_line_opacity.value

    XsAttributeValue {
        id: mask_line_thickness
        attributeTitle: "Line Thickness"
        model: mask_settings
    }
    property alias maskLineThickness: mask_line_thickness.value

    XsAttributeValue {
        id: label_size
        attributeTitle: "Label Size"
        model: mask_settings
    }
    property alias labelSize: label_size.value

    XsAttributeValue {
        id: show_mask_label
        attributeTitle: "Show Mask Label"
        model: mask_settings
    }
    property alias showMaskLabel: show_mask_label.value

    XsAttributeValue {
        id: render_method
        attributeTitle: "Mask Render Method"
        model: mask_settings
    }
    property alias renderMethod: render_method.value

    property bool mask_defined: maskAspectRatio > 0.0

    property var safety: safetyPercent/200.0

    property var l: width*safety
    property var b: (height-(width*(1.0-safety*maskAspectRatio)/maskAspectRatio))/2.0
    property var r: width*(1.0 - safety)
    property var t: (height+(width*(1.0-safety*maskAspectRatio)/maskAspectRatio))/2.0

    Item {

        anchors.fill: parent


        Rectangle {
            id: bottom_masking_rect
            opacity: maskOpacity
            color: "black"
            x: 0
            y: 0
            width: parent.width
            height: b
        }

        Rectangle {
            id: top_masking_rect
            opacity: maskOpacity
            color: "black"
            x: 0
            y: t
            width: parent.width
            height: parent.height-t
        }

        Rectangle {
            id: left_masking_rect
            opacity: maskOpacity
            color: "black"
            x: 0
            y: b
            width: l
            height: t-b
        }

        Rectangle {
            id: right_masking_rect
            opacity: maskOpacity
            color: "black"
            x: r
            y: b
            width: parent.width-r
            height: t-b
        }

        Rectangle {
            id: lines
            opacity: maskLineOpacity
            color: "transparent"
            border.color: "white"
            property var tt: maskLineThickness*scalingFactor
            border.width: tt
            x: l-tt/2
            y: b-tt/2
            width: r-l+tt
            height: t-b+tt
        }

        Text {
            text: mask_name
            opacity: maskLineOpacity
            visible: showMaskLabel
            color: "white"
            font.pixelSize: Math.round(labelSize*scalingFactor)
            anchors.left: lines.left
            anchors.bottom: lines.top
            anchors.bottomMargin: 4
        }
    }

}
