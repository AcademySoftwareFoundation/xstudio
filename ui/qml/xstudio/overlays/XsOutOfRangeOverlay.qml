// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQml 2.14

import xStudio 1.0

Rectangle {

    id: out_of_range_overlay
    visible: viewport.frameOutOfRange
    anchors.fill: parent
    color: "#aa000000"
    property var imageBox

    Text {
        x: imageBox.x + imageBox.width/2 - width/2
        y: imageBox.y + imageBox.height/2 - height/2
        text: "Out of\nRange"
        color: XsStyle.outOfRangeColor
        font.pixelSize: XsStyle.outOfRangeFontSize*imageBox.width/800
        font.family: XsStyle.outOfRangeFontFamily
        font.weight: XsStyle.outOfRangeFontWeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
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
            ctx.lineWidth = 6;
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
