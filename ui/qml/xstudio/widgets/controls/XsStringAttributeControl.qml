// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
            
XsTextField {
    
    id: input
    wrapMode: Text.Wrap
    clip: true
    focus: true
    horizontalAlignment: Qt.AlignLeft
    verticalAlignment: Qt.AlignVCenter
    
    onActiveFocusChanged:{
        if(activeFocus) selectAll()
    }

    property var backendValue: typeof value != "undefined" ? value : ""

    onBackendValueChanged: text = backendValue

    onTextChanged: {
        if (backendValue != text) {
            value = text
        }
    }

    background: Rectangle{
        color: input.activeFocus? Qt.darker(palette.highlight, 1.5): input.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
        border.width: input.hovered || input.active? 1:0
        border.color: palette.highlight
        opacity: enabled? 0.7 : 0.3
    }
}
