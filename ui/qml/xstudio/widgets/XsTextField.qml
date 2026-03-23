// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls.Basic
import QtQuick


import xStudio 1.0

TextField { id: widget

    property int textWidth: text==""? 0 : metrics.width + 3

    property color borderColor: XsStyleSheet.panelBgColor
    property alias bgColor: bg.color

    property alias bgVisibility: bg.visible
    property bool forcedHover: false

    opacity: enabled ? 1.0 : 0.5

    font.bold: true
    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    color: XsStyleSheet.primaryTextColor
    selectedTextColor: "white"
    selectionColor: XsStyleSheet.accentColor

    hoverEnabled: true
    horizontalAlignment: TextInput.AlignHCenter

    padding: 0
    selectByMouse: true
    activeFocusOnTab: true
    onEditingFinished: focus = false

    objectName: "XsSearchBar"

    TextMetrics {
        id: metrics
        font: widget.font
        text: widget.text
    }

    background:
    Rectangle {
        id: bg
        color: "transparent"
        border.color: widget.focus || widget.hovered || forcedHover ? XsStyleSheet.accentColor : borderColor
    }
}
