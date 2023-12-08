// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQml 2.14
import xstudio.qml.module 1.0

import xStudio 1.0

Rectangle {

    id: out_of_range_overlay
    anchors.fill: parent
    color: "#aa000000"
    property var imageBox
    property var frame_error_message: viewport_error_frame.frame_error ? viewport_error_frame.frame_error : ""
    visible: frame_error_message != ""

    XsModuleAttributes {
        id: viewport_error_frame
        attributesGroupNames: "viewport_frame_error_message"
    }

    Text {
        anchors.fill: parent
        text: frame_error_message
        color: XsStyle.frameErrorColor
        font.pixelSize: XsStyle.frameErrorFontSize
        font.family: XsStyle.frameErrorFontFamily
        font.weight: XsStyle.frameErrorFontWeight
        verticalAlignment: Text.AlignBottom
        horizontalAlignment: Text.AlignLeft
        wrapMode: Text.Wrap
        padding: 10
    }

    Canvas {

        property var imageBox: out_of_range_overlay.imageBox

        onImageBoxChanged: requestPaint()

        property var l: imageBox.x
        property var b: imageBox.y
        property var r: imageBox.x + imageBox.width
        property var t: imageBox.y + imageBox.height
        property var hw: imageBox.width/3
        property var hh: imageBox.height/3

        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = XsStyle.outOfRangeColor;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(l+hw/4, b+hh/4);
            ctx.lineTo(l+hw, b+hh);
            ctx.moveTo(r-hw/4, t-hh/4);
            ctx.lineTo(r-hw, t-hh);
            ctx.moveTo(r-hw/4, b+hh/4);
            ctx.lineTo(r-hw, b+hh);
            ctx.moveTo(l+hw/4, t-hh/4);
            ctx.lineTo(l+hw, t-hh);
            ctx.moveTo(l, b);
            ctx.lineTo(r, b);
            ctx.lineTo(r, t);
            ctx.lineTo(l, t);
            ctx.lineTo(l, b);
            ctx.stroke();
        }
    }
}
