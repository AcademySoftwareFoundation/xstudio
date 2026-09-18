// SPDX-License-Identifier: Apache-2.0
import QtQuick
import xStudio 1.0

Rectangle {
    property color flatColor: XsStyleSheet.panelBgGradTopColor
    property color topColor: XsStyleSheet.panelBgGradTopColor
    property color bottomColor: XsStyleSheet.baseColor
    property bool useFlatTheme: XsStyleSheet.useFlatTheme

    property alias orientation: grad.orientation

    property real topPosition: 0.0
    property real bottomPosition: 1.0

    opacity: enabled ? 1.0 : 0.33

    gradient: Gradient {
    	id: grad
        GradientStop { position: topPosition; color: useFlatTheme ? flatColor : topColor }
        GradientStop { position: bottomPosition; color: useFlatTheme ? flatColor : bottomColor }
    }
}