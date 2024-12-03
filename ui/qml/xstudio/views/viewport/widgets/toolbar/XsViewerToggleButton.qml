// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

XsViewerToolbarButtonBase {

    showBorder: mouseArea.containsMouse

    MouseArea{

        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            value = !value
        }

    }

}

