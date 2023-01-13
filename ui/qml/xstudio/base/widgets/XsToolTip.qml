// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1


ToolTip {
    id: widget
    
    property alias textDiv: textDiv
    property alias textDivMetrics: metrics

    delay: 100

    contentItem: Text {
        id: textDiv
        text: widget.text
        font: widget.font
        color: "black"
        width: parent.width
        wrapMode: Text.Wrap //WrapAnywhere
    }

    TextMetrics {
        id: metrics
        font: textDiv.font
        text: textDiv.text
    }

    background: Rectangle {
        border.color: "black"
    }
}