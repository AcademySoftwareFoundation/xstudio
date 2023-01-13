// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsPopup {
    id: popup

    property alias value: control.value
    property alias to: control.to
    property alias from: control.from
    property alias factor: control.factor
    property alias stepSize: control.stepSize
    property alias orientation: control.orientation
    property alias slider: control.slider

    XsIntSlider {
        id: control
        anchors.fill: parent

        onUnPressed: close()
    }
}
