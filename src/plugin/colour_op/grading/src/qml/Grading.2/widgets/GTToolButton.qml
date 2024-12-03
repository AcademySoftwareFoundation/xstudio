// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0

XsPrimaryButton{ id: widget

    property alias src: imgDiv.source

    text: ""
    imgSrc: ""
    textDiv.visible: false
    imageDiv.visible: false
    isToolTipEnabled: false

    XsText {
        text: parent.text
        horizontalAlignment: Text.AlignHCenter
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        anchors.top: parent.top
        anchors.topMargin: 3
    }
    XsImage {
        id: imgDiv
        opacity: enabled ? 1.0 : 0.33
        source: ""
        sourceSize.height: 25
        sourceSize.width: 25
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        imgOverlayColor: palette.text
    }

    onClicked:{
    }

}
