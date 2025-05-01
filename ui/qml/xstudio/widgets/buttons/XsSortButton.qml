// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts

import xStudio 1.0


XsPrimaryButton{

    text: ""
    imgSrc: ""
    textDiv.visible: false

    property alias sortIconText: sortCategoryText.text
    property alias sortIconSrc: sortCategoryIcon.source
    property bool isDescendingOrder: false

    XsIcon{ id: sortCategoryIcon
        source: ""
        width: parent.width/2
        height: width
        anchors.verticalCenter: parent.verticalCenter
        x: 5
    }
    XsText{ id: sortCategoryText
        text: ""
        width: parent.width/2
        height: parent.height
        font.pixelSize: XsStyleSheet.fontSize*1.2
        anchors.verticalCenter: parent.verticalCenter
        font.bold: true
        x: 5
    }

    XsIcon{ id: sortOrderIcon
        source: "qrc:///shotbrowser_icons/up_arrow.svg"
        rotation: isDescendingOrder? 180 : 0
        width: parent.width/2
        height: parent.height*0.7
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
    }

}