// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
Item {

    XsAttributeValue {
        id: __status
        attributeTitle: "Status"
        model: decklink_settings
    }
    property alias statusMessage: __status.value

    XsAttributeValue {
        id: __inError
        attributeTitle: "In Error"
        model: decklink_settings
    }
    property alias inError: __inError.value
    
    XsImage {

        anchors.fill: parent
        source: inError ? "qrc:/bmd_icons/status-error.svg" : "qrc:/bmd_icons/status-ok.svg"
        imgOverlayColor: inError ? "red": "green" 
        opacity: ma.containsMouse ? 1 : 0.8
        antialiasing: true
        smooth: true

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                tooltip.visible = true
            }
        }

    }

    XsToolTip {
        id: tooltip
        text: statusMessage
        visible: ma.containsMouse
    }

}
