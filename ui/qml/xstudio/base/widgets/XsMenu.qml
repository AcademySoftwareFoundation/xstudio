// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

Menu {
    id:myMenu

    //    property alias topLeftCorner: bgrect.topLeftCorner
    //    property alias topRightCorner: bgrect.topRightCorner
    //    property alias bottomRightCorner: bgrect.bottomRightCorner
    //    property alias bottomLeftCorner: bgrect.bottomLeftCorner

    //    background: RoundedRectangle {
    property alias radius: bgrect.radius
    property bool hasCheckable: false
    property bool fakeDisabled: false
    topPadding: Math.max(3, XsStyle.menuRadius)
    bottomPadding: topPadding
    dim: false
    
    background: Rectangle {
        id: bgrect
        border {
            color: XsStyle.menuBorderColor
            width: XsStyle.menuBorderWidth
        }
        color: XsStyle.mainBackground
        radius: XsStyle.menuRadius
    }
    width: {
        var result = 0;
        var padding = 0;
        for (var i = 0; i < count; ++i) {
            var item = itemAt(i);
            if (item.contentItem != undefined) {
                result = Math.max(item.contentItem.implicitWidth, result);
                padding = Math.max(item.padding, padding);
            }
        }
        return Math.max(100, result + padding * 2);
    }
    delegate: XsMenuItem {}
    Component.onCompleted: {
        var curritem;
        for (var i=0; i<myMenu.count; i++) {
            curritem = myMenu.itemAt(i);
            if (curritem.mycheckable) {
                hasCheckable = true
                i = myMenu.count
            }
        }
    }

   function toggleShow() {
        if(visible)
            close()
        else
            open()
    }
}

