// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.0

TextField { id: widget

    signal editingCompleted()

    property bool bgVisibility: true
    property bool forcedHover: false
    property color bgColor: palette.base
    property color iconOverlayColor: "#959595"

    property color textColorSelection: palette.text
    property color textColorEditing: palette.text
    property color textColorNormal: palette.text
    property color textColorHint: "#C1C1C1"
    property color selectionColorEditing: palette.highlight

    property color borderColorHovered: palette.highlight
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    color: text==""? textColorHint : focus || hovered? textColorEditing: textColorNormal
    selectedTextColor: textColorSelection
    selectionColor: selectionColorEditing

    hoverEnabled: true
    selectByMouse: true
    activeFocusOnTab: true
    opacity: widget.enabled? 1 : 0.3
    horizontalAlignment: TextInput.AlignLeft
    rightPadding: clearBtn.width + clearBtn.anchors.rightMargin*2


    onEditingFinished: {
        focus = false
        editingCompleted()
    }

    // TextMetrics { //#TODO: elide
    //     id: metricsDiv
    //     font: widget.font
    //     text: widget.text
    //     elideWidth: widget.width - (widget.leftPadding + widget.rightPadding)
    // }

    background:
    Rectangle {
        visible: bgVisibility
        implicitWidth: 100
        implicitHeight: 28
        border.color: widget.hovered || forcedHover? borderColorHovered: borderColorNormal
        border.width: 1
        color: bgColor

        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: widget.activeFocus || widget.focus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
    }

    XsSecondaryButton{ id: clearBtn
        width: visible? 16 : 0
        height: 16
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        imgSrc: "qrc:/icons/close.svg"
        visible: widget.length!=0

        onClicked: {
            clearSearch()
            widget.focus = true
        }
    }


    function clearSearch()
    {
        widget.text=""
    }

}
