// SPDX-License-Identifier: Apache-2.0
import QtQuick



import xStudio 1.0

TextInput{
    id: widget

    text: ""

    property color highlightColor: palette.highlight
    property color textColorNormal: palette.text
    property color textColorSelected: "black"

    color: textColorNormal

    clip: true
    height: parent.height
    verticalAlignment: TextEdit.AlignVCenter

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    selectByMouse: true
    readOnly: false

    selectedTextColor: textColorSelected
    selectionColor: highlightColor

    onActiveFocusChanged: {
        if(activeFocus) selectAll()
    }
}