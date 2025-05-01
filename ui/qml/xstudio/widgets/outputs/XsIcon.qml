// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects

import xStudio 1.0

Image {

    id: img
    property color imgOverlayColor: XsStyleSheet.primaryTextColor
    
    sourceSize.height: height
    sourceSize.width: width
    
    smooth: true
    antialiasing: true
    asynchronous: true

    layer {
        enabled: true
        effect: 
        MultiEffect {
            source: img
            brightness: 1.0
            colorization: 1.0
            colorizationColor: imgOverlayColor
        }
    }

}