// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
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
    width: view.width
    height: view.height
    property var imageBoxes: view.imageBoundariesInViewport
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

    Repeater {

        model: view.imageBoundariesInViewport
        Item {

            // Viewport class provides imageBoxes - the coordinates of each
            // image within the viewport, in viewport pixels
            property var imageBox: imageBoxes[index] ? imageBoxes[index] : Qt.QRectF()

            x: imageBox.x
            y: imageBox.y
            height: imageBox.height
            width: imageBox.width

            property var l: width*safety
            property var b: (height-(width*(1.0-safety*maskAspectRatio)/maskAspectRatio))/2.0
            property var r: width*(1.0 - safety)
            property var t: (height+(width*(1.0-safety*maskAspectRatio)/maskAspectRatio))/2.0

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
                border.width: maskLineThickness
                x: l-maskLineThickness/2
                y: b-maskLineThickness/2
                width: r-l+maskLineThickness
                height: t-b+maskLineThickness
            }

            Text {
                text: mask_name
                opacity: maskLineOpacity
                visible: showMaskLabel
                color: "white"
                font.pixelSize: labelSize
                anchors.left: lines.left
                anchors.bottom: lines.top
                anchors.bottomMargin: 4
            }
        }
    }

}
