// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 1.4
import QtGraphicalEffects 1.15

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