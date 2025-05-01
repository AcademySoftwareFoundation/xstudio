// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls.Basic
import QtQuick

import xStudio 1.0

Item {

    // Note: XsToolTip is added to some of our base item types (like
    // XsPrimaryButton). Each QML ToolTip is a PopUp, and therefore a Window.
    // On Windows10 platform, at least, having lots of hidden Window instances
    // kills performance due to the QAccessible component of Qt.
    // Therefore, and also for general efficiency, we only make the ToolTip
    // on demand via a Loader

    id: widget

    property var text

    width: 200
    property var delay: 500
    property var scaling: 1.0
    property var fixedWidth: 0.0
    property var maxWidth: 1024
    property var y_reposition: 0.0
    visible: false

    onVisibleChanged: {
        if (visible) {
            loader.sourceComponent = biscuit
            loader.item.x = 0
            loader.item.y = loader.item.height*y_reposition
            showTimer.running = false
            showTimer.running = true    
        } else {
            showTimer.running = false
            loader.sourceComponent = undefined
        }
    }

    Loader {
        id: loader
    }

    function show() {
        if (!visible) {
            visible = true
        } else {
            loader.sourceComponent = biscuit
            loader.item.x = 0
            loader.item.y = loader.item.height*y_reposition
            showTimer.running = false
            showTimer.running = true    
        }
    }

    function hide() {
        if (visible) {
            visible = false
        } else {
            loader.sourceComponent = undefined
        }
    }

    Timer {
        id: showTimer
        interval: delay
        running: false
        repeat: false
        onTriggered: {
        	loader.item.visible = true
        }
    }


    Component {

        id: biscuit

        ToolTip {

            id: tt

            text: widget.text
            property color bgColor: palette.text
            property color textColor: palette.base
        
            visible: false
        
            font.pixelSize: XsStyleSheet.fontSize*scaling
            font.family: XsStyleSheet.fontFamily

            property var autoWidth: fixedWidth != 0.0 ? fixedWidth : dummyText.contentWidth + (XsStyleSheet.panelPadding*3)

            width: autoWidth > maxWidth ? maxWidth : autoWidth
            height: dummyText2.height + XsStyleSheet.panelPadding*2
        
            rightPadding: XsStyleSheet.panelPadding
            leftPadding: XsStyleSheet.panelPadding

            contentItem: Text {
                id: textDiv
                text: tt.text
                font: tt.font
                color: palette.base
                wrapMode: Text.Wrap
            }

            // We need this to get the 'natural' width of the rendered text
            // without wrapping, in case this is less than the maxWidth or
            // fixedWidth
            Text {
                id: dummyText
                text: tt.text
                font: tt.font
                opacity: 0
            }

            Text {
                width: textDiv.width
                id: dummyText2
                text: tt.text
                font: tt.font
                wrapMode: Text.Wrap
                opacity: 0
            }
        
            // This background shadow doesn't work, you can't see it 
            // probably because it is clipped by the parent.
            /*background: Rectangle {

                color: palette.text

                Rectangle {
                    id: shadowDiv
                    color: "#000000"
                    opacity: 0.2
                    x: 20
                    y: -20
                    z: -1
                }
            }*/

        }

    }


}