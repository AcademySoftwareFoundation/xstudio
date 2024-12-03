// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.0

TextField { id: widget

    property color bgColorEditing: palette.highlight
    property color bgColorNormal: palette.base

    property color textColorSelection: palette.text
    property color textColorEditing: palette.text
    property color textColorNormal: "light grey"
    property color textColor: palette.text
    property real textWidth: text==""? 0 : metrics.width+3

    property color borderColor: palette.base
    property real borderWidth: 1
    property color bgColor: "transparent" //palette.base

    property bool bgVisibility: true
    property bool forcedBg: false
    property bool forcedHover: false

    opacity: enabled ? 1.0 : 0.5

    signal editingCompleted()

    font.bold: true
    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    color: textColor //enabled? focus || hovered? textColorEditing: textColorNormal: Qt.darker(textColorNormal, 1.75)
    selectedTextColor: "white"
    selectionColor: palette.highlight

    hoverEnabled: true
    horizontalAlignment: TextInput.AlignHCenter

    padding: 0
    selectByMouse: true
    activeFocusOnTab: true
    onEditingFinished: {
        editingCompleted()
    }

    TextMetrics {
        id: metrics
        font: widget.font
        text: widget.text
    }

    background:
    Rectangle {
        visible: bgVisibility
        color: bgColor //enabled || forcedBg? widget.focus? Qt.darker(bgColorEditing, 2.75): bgColorNormal: Qt.darker(bgColorNormal, 1.75)
        border.color: widget.focus || widget.hovered || forcedHover? bgColorEditing: borderColor
    }

}
