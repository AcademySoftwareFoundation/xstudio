// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects

import xStudio 1.0

XsImage {
    id: img

    property color imgOverlayColor: XsStyleSheet.primaryTextColor

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