// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.viewport 1.0

XsViewerToolbarButtonBase {

    showBorder: mouseArea.containsMouse
    anchors.fill: parent

    text: "FPS"
    valueText: value
    property var popup_visible: false

    MouseArea{

        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onPressed: {
            if (!popup_visible) {
                loader.sourceComponent = popup
                loader.item.visible = true
                popup_visible = true
            }
        }

    }

    Loader {
        id: loader
    }

    Component {
        id: popup

        XsViewerSetMediaFPSPopup {

            id: the_popup
            x: 0
            y: -height //+(width/1.25)
            onVisibleChanged: {

                if (!visible) {
                    // awkward! If the popup is visible and the user clicks on
                    // the FPS toolbar button to hide it, it is auto-hidden by
                    // QML before we get to hide it ourselves. So the onPressed
                    // slot above would show the popup again immediately. We 
                    // need a wee timeout callback here so the FPS button can 
                    // be used to toggle this pop-up on/off
                    callbackTimer.setTimeout(function() { return function() {
                        popup_visible = false
                        }}(), 100);
                }
            }

        }

    }

}

