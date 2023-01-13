// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QuickFuture 1.0
import QuickPromise 1.0
import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    id: control
    color: "transparent"
    width: viewport.width
    height: viewport.height

    property var imageBox: viewport.imageBoundaryInViewport

    onImageBoxChanged: {
        computeMask()
    }

    XsModuleAttributes {
        
        id: mask_settings
        attributesGroupName: "dnmask_settings"
        onAttrAdded: control.computeMask()
        onValueChanged: control.computeMask()

    }

    XsModuleAttributes {
        
        id: current_mask_spec
        attributesGroupName: "dnmask_values"
        onAttrAdded: control.computeMask()
        onValueChanged: control.computeMask()

    }


    
    // Properties on the XsModuleAttributes items are created at runtime after
    // this item is 'completed' and therefore we need to map to local variables
    // with checks as follows:
    property var mask_aspect: current_mask_spec.mask_aspect ? current_mask_spec.mask_aspect : 0.0
    property var safety_percent: current_mask_spec.safety_percent ? current_mask_spec.safety_percent : 0.0
    property var mask_opacity: mask_settings.mask_opacity ? mask_settings.mask_opacity : 0.0
    property var mask_line_opacity: mask_settings.mask_line_opacity ? mask_settings.mask_line_opacity : 0.0
    property var mask_line_thickness: mask_settings.mask_line_thickness ? mask_settings.mask_line_thickness : 0.0
    property var mask_name: current_mask_spec.mask ? current_mask_spec.mask : ""
    property var show_mask_label: mask_settings.show_mask_label ? mask_settings.show_mask_label : false
    property bool mask_defined: false

    property var l: 0.0
    property var b: 0.0
    property var r: width
    property var t: height

    visible: mask_defined

    function computeMask() {

        if (mask_aspect == undefined || mask_aspect == 0.0) {
            mask_defined = false
            return;
        } else {
            mask_defined = true
        }

        // assuming mask should be 'width fitted'
        l = imageBox.x
        r = imageBox.x + imageBox.width
        var h = imageBox.width/mask_aspect
        b = imageBox.y + (imageBox.height-h)/2.0
        t = imageBox.y + imageBox.height/2 + h/2.0

        if (safety_percent != 0.0) {
            var xd = (r-l)*safety_percent/100.0
            l = l + xd
            r = r - xd
            var yd = (t-b)*safety_percent/100.0
            b = b + yd
            t = t - yd
        } 

    }

    Rectangle {
        id: bottom_masking_rect
        opacity: mask_opacity
        color: "black"
        x: 0
        y: 0
        width: control.width
        height: b
    }

    Rectangle {
        id: top_masking_rect
        opacity: mask_opacity
        color: "black"
        x: 0
        y: t
        width: control.width
        height: control.height-t
    }

    Rectangle {
        id: left_masking_rect
        opacity: mask_opacity
        color: "black"
        x: 0
        y: b
        width: l
        height: t-b
    }

    Rectangle {
        id: right_masking_rect
        opacity: mask_opacity
        color: "black"
        x: r
        y: b
        width: control.width-x
        height: t-b
    }

    Rectangle {
        id: lines
        opacity: mask_line_opacity
        color: "transparent"
        border.color: "white"
        border.width: mask_line_thickness
        x: l-mask_line_thickness/2
        y: b-mask_line_thickness/2
        width: r-l+mask_line_thickness
        height: t-b+mask_line_thickness
    }

    Text {
        text: mask_name
        opacity: mask_line_opacity
        visible: show_mask_label
        color: "white"
        anchors.left: lines.left
        anchors.bottom: lines.top
        anchors.bottomMargin: 4
    }

}
