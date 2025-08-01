// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.1

TextField { id: widget

    property color bgColorEditing: palette.highlight
    property color bgColorNormal: palette.base

    property color textColorSelection: palette.text
    property color textColorEditing: palette.text
    property color textColorNormal: "light grey"

    property color borderColor: palette.base
    property real borderWidth: 1
    property real fontSize: XsStyle.menuFontSize

    property bool bgVisibility: true
    property bool forcedBg: false
    property bool forcedHover: false

    signal editingCompleted()

    font.pixelSize: fontSize
    font.family: XsStyle.fontFamily
    color: enabled? focus || hovered? textColorEditing: textColorNormal: Qt.darker(textColorNormal, 1.75)
    selectedTextColor: "white"
    selectionColor: palette.highlight

    hoverEnabled: true
    horizontalAlignment: TextInput.AlignHCenter

    padding: 0
    selectByMouse: true
    activeFocusOnTab: true
    onEditingFinished: {
        // focus = false //#TODO: conflicting behavior in SG_Browser and PublishNotes.
        editingCompleted()
    }

    background:
    Rectangle {
        visible: bgVisibility
        implicitWidth: width
        implicitHeight: height
        color: enabled || forcedBg? widget.focus? Qt.darker(bgColorEditing, 2.75): bgColorNormal: Qt.darker(bgColorNormal, 1.75)
        border.color: widget.focus || widget.hovered || forcedHover? bgColorEditing: borderColor
    }

}
