// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Image {
    id: widget

    property color imgOverlayColor: palette.text
    
    source: ""
    // width: parent.height-4
    // height: parent.height-4
    // topPadding: 2
    // bottomPadding: 2
    // leftPadding: 8
    // rightPadding: 8

    // sourceSize.height: 24
    // sourceSize.width: 24

    horizontalAlignment: Image.AlignHCenter
    verticalAlignment: Image.AlignVCenter
    
    smooth: true
    antialiasing: true

    layer {
        enabled: true
        effect: ColorOverlay { color: imgOverlayColor }
    }
}