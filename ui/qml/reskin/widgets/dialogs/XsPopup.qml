import QtQuick 2.15
import QtQuick.Controls 2.15

import xStudioReskin 1.0

Popup {

    id: widget
    topPadding: XsStyleSheet.menuPadding
    bottomPadding: XsStyleSheet.menuPadding
    leftPadding: 0
    rightPadding: 0

    // parent: Overlay.overlay //#TODO

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "#4B4B4B" //"#5C5C5C"
    property color forcedBgColorNormal: bgColorNormal //"#E6676767"

    background: Rectangle{
        implicitWidth: 100
        implicitHeight: 200
        border.width: forcedBgColorNormal==bgColorNormal? 0:1
        border.color: XsStyleSheet.baseColor
        gradient: Gradient {
            GradientStop { position: 0.0; color: forcedBgColorNormal==bgColorNormal?"#707070":"#F2676767" }
            GradientStop { position: 1.0; color: forcedBgColorNormal }
        }
    }

}


