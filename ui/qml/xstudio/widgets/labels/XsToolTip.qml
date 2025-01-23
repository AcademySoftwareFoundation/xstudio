// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

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
    property var y_reposition: 0.0

    onVisibleChanged: {
        if (visible) {
            loader.sourceComponent = biscuit
            loader.item.visible = true
            loader.item.x = 0
            loader.item.y = loader.item.height*y_reposition
        } else {
            loader.sourceComponent = undefined
        }
    }    

    Loader {
        id: loader
    }


    Component {

        id: biscuit

        ToolTip {

            id: tt

            text: widget.text
                
            property color bgColor: palette.text
            property color textColor: palette.base
        
            delay: widget.delay
        
            font.pixelSize: XsStyleSheet.fontSize*scaling
            font.family: XsStyleSheet.fontFamily
        
            width: fixedWidth != 0.0 ? fixedWidth : metricsDiv.width + (XsStyleSheet.panelPadding*3)
        
            rightPadding: XsStyleSheet.panelPadding
            leftPadding: XsStyleSheet.panelPadding
        
            TextMetrics {
                id: metricsDiv
                font: tt.font
                text: tt.text
            }
        
            contentItem: Text {
                id: textDiv
                text: tt.text
                font: tt.font
                color: palette.base
                wrapMode: Text.Wrap
            }
        
            background: Rectangle {

                color: palette.text
        
                Rectangle {
                    id: shadowDiv
                    color: "#000000"
                    opacity: 0.2
                    x: 2
                    y: -2
                    z: -1
                }
            }
        }
        
    }


}