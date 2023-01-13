// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.0

TextEdit {
	id: text_edit
    property string placeholderText: "Enter text here..."
    property alias textDiv: textDiv

    color: palette.text
    selectByMouse: true

	font {
        pixelSize: XsStyle.popupControlFontSize
        family: XsStyle.controlContentFontFamily
        hintingPreference: Font.PreferNoHinting
    }
	verticalAlignment: Qt.AlignVCenter

    Text {
        id: textDiv
	    leftPadding: parent.leftPadding
	    rightPadding: parent.rightPadding
	    topPadding: parent.topPadding
	    bottomPadding: parent.topPadding
    	anchors.fill: parent
        verticalAlignment: text_edit.verticalAlignment
        horizontalAlignment: text_edit.horizontalAlignment
        text: text_edit.placeholderText
        font.italic: true
        // color: palette.highlightedText
        opacity: 0.8
        visible: !text_edit.text
        elide: Text.ElideRight
    }
}