// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects

import xStudio 1.0

Image {
    id: widget
    property color imgOverlayColor:"transparent"

    sourceSize.height: height
    sourceSize.width: width

    horizontalAlignment: Image.AlignHCenter
    verticalAlignment: Image.AlignVCenter

    smooth: true
    antialiasing: true
    asynchronous: true

}