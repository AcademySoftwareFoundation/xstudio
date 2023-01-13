// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.0

Button {
    property var tooltip: ""
    property var tooltipTitle: ""
    implicitHeight: control.implicitHeight
    implicitWidth: control.implicitWidth
    hoverEnabled: true

    onHoveredChanged: {
        if(hovered) {
            status_bar.normalMessage(tooltip, tooltipTitle)
        } else {
            status_bar.clearMessage()
        }
    }

    background: Rectangle {
        color: hovered ? palette.highlight : (highlighted ? palette.highlight : palette.button)
        gradient: hovered ? styleGradient.accent_gradient : Gradient.gradient
        anchors.fill: parent
    }

    contentItem: Label {
        id: control
        anchors.centerIn: parent
        text: parent.text
        color: palette.text
        font.family: XsStyle.fontFamily
        font.pixelSize: 12
        font.hintingPreference: Font.PreferNoHinting
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
