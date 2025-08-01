// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Text {
    id: widget

    property color textColorNormal: palette.text
    property var textElide: widget.elide
    property real textWidth: metrics.width
    property alias toolTip: toolTip
    property alias tooltipText: toolTip.text
    property alias tooltipVisibility: toolTip.visible
    property real toolTipWidth: widget.width+5 //150

    text: ""
    color: textColorNormal 

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    // topPadding: 2
    // bottomPadding: 2
    // leftPadding: 20
    // rightPadding: 20
    elide: Text.ElideRight
    // width: parent.width
    // height: parent.height

    TextMetrics {
        id: metrics
        font: widget.font
        text: widget.text
    }
    // MouseArea{
    //     id: mArea
    //     anchors.fill: parent
    //     hoverEnabled: true
    //     propagateComposedEvents: true
    // }

    XsToolTip{
        id: toolTip
        text: widget.text
        visible: false //mArea.containsMouse && parent.truncated
        width: metrics.width == 0? 0 : toolTipWidth
        x: 0 //#TODO: flex/pointer
    }
}