// SPDX-License-Identifier: Apache-2.0
import QtQuick
import xStudio 1.0

Rectangle { id: widget

    property color flatColor: XsStyleSheet.panelBgFlatColor
    property color topColor: XsStyleSheet.panelBgGradTopColor
    property color bottomColor: XsStyleSheet.panelBgGradBottomColor
    property alias orientation: grad.orientation
    property bool flatTheme: isFlatTheme

    property real topPosition: 0.0
    property real bottomPosition: 1.0

    opacity: enabled? 1.0 : 0.33

    gradient: Gradient {
    	id: grad
        GradientStop { position: topPosition; color: flatTheme ? flatColor : topColor }
        GradientStop { position: bottomPosition; color: flatTheme ? flatColor : bottomColor }
    }

}