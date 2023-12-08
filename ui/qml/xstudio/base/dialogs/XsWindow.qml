// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

import xStudio 1.0

ApplicationWindow {
    id: control
    property bool onTop: true
    property bool frameLess: false
    property bool asDialog: false
    property bool asWindow: false
    property var centerOn: app_window
    property bool keepCentered: false
    property bool centerOnOpen: false

    // override default palette
    palette.base: XsStyle.controlBackground
    palette.text: XsStyle.hoverColor
    palette.button: XsStyle.controlTitleColor
    palette.highlight: XsStyle.highlightColor
    palette.light: XsStyle.highlightColor
    palette.highlightedText: XsStyle.mainBackground

    palette.brightText: XsStyle.highlightColor
    palette.buttonText: XsStyle.hoverColor
    palette.windowText: XsStyle.hoverColor

    function toggle() {
        if(visible) {
            hide()
        } else {
            show()
        }
    }

    Connections {
        target: control
        function onVisibleChanged() {
            if(!visible) {
                app_window.requestActivate()
                sessionWidget.playerWidget.forceActiveFocus()
            } else {
                control.requestActivate()
                if(keepCentered || centerOnOpen) {
                    centerOnOpen = false
                    if(centerOn) {
                        let xx = centerOn.x
                        if(typeof centerOn.mapToGlobal !== "undefined"){
                            xx = centerOn.mapToGlobal(0, 0).x
                        }
                        x = Math.max(xx, xx + (centerOn.width/2) - (width / 2))

                        let yy = centerOn.y
                        if(typeof centerOn.mapToGlobal !== "undefined"){
                            yy = centerOn.mapToGlobal(0, 0).y
                        }
                        y = Math.max(yy, yy + (centerOn.height/2) - (height / 2))
                    } else {
                        x = (Screen.width / 2) - (width / 2)
                        y = (Screen.height / 2) - (height / 2)
                    }
                }
            }
        }
    }

    flags: (asDialog ? Qt.Dialog : asWindow ? Qt.WindowSystemMenuHint : Qt.Tool) |(frameLess ? Qt.FramelessWindowHint : 0) | (onTop ? Qt.WindowStaysOnTopHint : 0)
    color: "#222"
}
