// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0
import xstudio.qml.bookmarks 1.0

Item { id: widget

    property string valueText: ""
    property alias currentText: textField.text
    property bool isHovered: mArea.containsMouse
    property bool isPressed: false

    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color bgColorPressed: palette.highlight
    property color indicatorColor: "transparent"
    property real borderWidth: 1
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"

    signal edited()

    XsGradientRectangle { id: bgDiv

        anchors.fill: parent
        border.color: widget.down || widget.isHovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth

        opacity: enabled? 1.0 : 0.33
        flatColor: topColor
        topColor: isPressed? bgColorPressed: bgColorNormal
        bottomColor: isPressed? bgColorPressed: bgColorNormal

        Rectangle{ id: activeIndicator
            anchors.bottom: parent.bottom
            width: borderWidth*5
            height: parent.height
            color: indicatorColor
        }
    }

    MouseArea{ id: mArea
        anchors.fill: parent
        hoverEnabled: true
    }

    XsTextField{ id: textField
        width: parent.width
        height: parent.height
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        text: valueText
        font.bold: true

        bgColorNormal: "transparent"
        borderColor: bgColorNormal
        // validator: DoubleValidator {  //to fix support for GTSliderItem & GTWheelItem
        //     bottom: float_scrub_min[index]
        // }
        onEditingCompleted: {
            edited()
        }
    }

}
