// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQml 2.14

import xStudio 1.0

Rectangle {

    id: viewport_blank_card
    anchors.fill: parent
    property var text: "Welcome to xStudio"

    color: "black"

    property bool first_time: true
    onVisibleChanged: {
        if (!visible) {
            if (!first_time) {
                text = "No Media"
            }
        } else {
            first_time = false
        }
    }

    Text {
        id: message
        anchors.centerIn: parent
        text: viewport_blank_card.text
        color: XsStyle.blankViewportColor
        font.pixelSize: XsStyle.blankViewportMessageSize
        font.family: XsStyle.controlContentFontFamily
        font.weight: Font.Thin
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    TextMetrics {
        id: textMetrics
        font: message.font
        text: message.text
    }

    Canvas {

        property var border: 20

        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = XsStyle.blankViewportColor;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(width/2,border);
            ctx.lineTo(width/2,height/2-textMetrics.height/2-border);
            ctx.moveTo(width/2,height/2+textMetrics.height/2+border);
            ctx.lineTo(width/2,height-border);
            ctx.moveTo(border,height/2);
            ctx.lineTo(width/2-textMetrics.width/2-border,height/2);
            ctx.moveTo(width/2+textMetrics.width/2+border,height/2);
            ctx.lineTo(width-border,height/2);
            ctx.stroke();
        }
    }
}
