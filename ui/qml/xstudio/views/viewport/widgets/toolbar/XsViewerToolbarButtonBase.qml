// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: widget

    property string text
    property string valueText
    property string shortText: text.substring(0,3)
    property string shortValue: valueText.substring(0,3) + "..."
    property bool isActive: false
    property bool isBgGradientVisible: true
    property bool showBorder: false
    property bool disabled : typeof attr_enabled != 'undefined' ? !attr_enabled : false

    XsGradientRectangle {
        visible: isBgGradientVisible
        anchors.fill: parent
        border.color: showBorder ? palette.highlight: "transparent"
        border.width: 1

        flatColor: topColor
        topColor: isActive ? palette.highlight : XsStyleSheet.controlColour
        bottomColor: isActive ? palette.highlight : "#1AFFFFFF"
    }

    TextMetrics {
        id: titleTextMetrics
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        text: widget.text
    }
    TextMetrics {
        id: valueDisplayMetrics
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        text: widget.valueText
    }
    TextMetrics {
        id: shortTitleMetrics
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        text: widget.shortText
    }
    TextMetrics {
        id: shortValueDisplayMetrics
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        text: widget.shortValue
    }

    property var full_width: titleTextMetrics.width + valueDisplayMetrics.width + layout.spacing
    property var short_title_full_value_width: shortTitleMetrics.width + valueDisplayMetrics.width + layout.spacing
    property var short_value_width: shortTitleMetrics.width + shortValueDisplayMetrics.width + layout.spacing

    property var displayText: width > full_width ? text : shortText
    property var displayValue: width > full_width ? valueText : (width > short_title_full_value_width) ? valueText : (width < short_value_width) ? "" : shortValue

    RowLayout {

        id: layout
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        XsText
        {
            id: titleDisplay
            Layout.fillHeight: true
            text: widget.displayText
            color: XsStyleSheet.secondaryTextColor
            horizontalAlignment: Text.AlignRight
            clip: true
        }

        XsText{

            id: valueDisplay
            Layout.fillHeight: true
            text: displayValue
            color: disabled ? XsStyleSheet.secondaryTextColor : palette.text
            horizontalAlignment: Text.AlignRight
            clip: true
            font.bold: true
        }

    }


}
