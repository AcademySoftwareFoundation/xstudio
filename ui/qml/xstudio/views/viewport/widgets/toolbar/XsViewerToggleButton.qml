// SPDX-License-Identifier: Apache-2.0
import QtQuick



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

