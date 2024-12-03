// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

Menu {
    id: widget

    dim: false
    topPadding: 4
    bottomPadding: 4

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    property bool hasCheckable: false
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: bgColorNormal

    background: Rectangle {
        id: bgrect
        gradient: Gradient {
            GradientStop { position: 0.0; color: XsStyleSheet.controlColour }
            GradientStop { position: 1.0; color: forcedBgColorNormal }
        }
    }

    delegate: XsMenuItem{}

    function toggleShow() {
        if(visible) close()
        else open()
    }





    Component.onCompleted: {
        var curritem;
        for (var i=0; i<widget.count; i++) {
            curritem = widget.itemAt(i);
            if (curritem.isCheckable) {
                hasCheckable = true
                i = widget.count
            }
        }
    }



}

