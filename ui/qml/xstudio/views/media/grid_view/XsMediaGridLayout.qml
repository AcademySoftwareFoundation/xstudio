// SPDX-License-Identifier: Apache-2.0
import QtQuick




import QtQuick.Layouts

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import "./delegates"

Item {


    Rectangle{ id: resultsBg
        anchors.fill: parent
        color: XsStyleSheet.panelBgColor
    }

    XsMediaGrid{

        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        anchors.leftMargin: 10

        model: mediaListModelData

    }



}
