// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Button {
    id: widget

    property alias imgSrc: imageDiv.source
    property bool isActive: false
    property bool isUnClickable: false
    property bool isActiveViaIndicator: true
    property bool isActiveIndicatorAtLeft: false
    property bool isToolTipEnabled: true
    property bool showBoth: false
    property bool isHovered: false

    property string toolTip: widget.text
    property string hotkeyNameForTooltip

    property alias textDiv: textDiv
    property alias imageDiv: imageDiv
    property alias bgDiv: bgDiv
    property alias activeIndicator: activeIndicator
    property real rotationAnimDuration: 150

    property alias imgOverlayColor: imageDiv.imgOverlayColor //palette.text
    property color textColor: palette.text
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    font.pixelSize: XsStyleSheet.fontSize *1.1
    font.weight: Font.Medium

    implicitWidth: textDiv.textWidth + 10

    onHoveredChanged: {
        if(hovered) {
            isHovered = true
        } else {
            isHovered = false
        }
    }

    contentItem:
    Item {
        anchors.fill: parent
        opacity: enabled || isUnClickable? 1.0 : 0.33

        Row {
            anchors.centerIn: parent

            width: Math.min(parent.width, (imageDiv.visible ? imageDiv.width:0) + (textDiv.visible ? textDiv.textWidth+10:0))
            height: parent.height

            XsImage {
                id: imageDiv
                anchors.verticalCenter: parent.verticalCenter
                // anchors.horizontalCenter: showBoth ? undefined : parent.horizontalCenter
                // anchors.right: showBoth ? textDiv.left : undefined
                // anchors.rightMargin: showBoth ? 10 : 0

                width: 20
                height: 20
                antialiasing: true
                smooth: true
                visible: source && source != ""
                imgOverlayColor: palette.text

                // Behavior on rotation {NumberAnimation{duration: rotationAnimDuration }}
            }

            XsText {
                id: textDiv
                visible: (!imgSrc | imgSrc=="")  | showBoth
                text: widget.text
                font: widget.font
                color: textColor
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
                // anchors.horizontalCenter: showBoth ? undefined : parent.horizontalCenter
                // anchors.centerIn: parent
                width: Math.min(parent.width-(imageDiv.visible ? imageDiv.width:0), textDiv.textWidth+10)
            }
        }
    }

    XsToolTip{
        text: toolTip
        visible: textDiv.visible && isToolTipEnabled? widget.isHovered && (textDiv.truncated || toolTip!= widget.text) : widget.isHovered && toolTip!=""
        onVisibleChanged: {
            if (visible) {
                // evaluate hotkey sequence when shown, as it may have been changed
                // by the user via their hotkey prefs
                var hk = ""
                if (hotkeyNameForTooltip != "") {
                    hk = hotkeysModel.hotkey_sequence_from_hotkey_name(hotkeyNameForTooltip)
                } else if (typeof hotkey_uuid !== "undefined" && hotkey_uuid) {
                    hk = hotkeysModel.hotkey_sequence(hotkey_uuid)
                }
                if (hk != "") {
                    text = toolTip + " (Hotkey = '" + hk + "' )"
                }
            }
        }
        // width: metricsDiv.width == 0? 0 : textDiv.textWidth +15
        // height: widget.height
        x: widget.width //#TODO: flex/pointer
        y: widget.height
    }

    background:
    XsGradientRectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: widget.down || widget.isHovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth

        flatColor: topColor
        topColor: widget.down || (isActive && !isActiveViaIndicator)? bgColorPressed: forcedBgColorNormal==bgColorNormal? XsStyleSheet.controlColour:forcedBgColorNormal
        bottomColor: widget.down || (isActive && !isActiveViaIndicator)? bgColorPressed: forcedBgColorNormal

        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: widget.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
        Rectangle{ id: activeIndicator
            anchors.bottom: parent.bottom
            width: isActiveIndicatorAtLeft? borderWidth*3 : parent.width;
            height: isActiveIndicatorAtLeft? parent.height : borderWidth*3
            color: isActiveViaIndicator && isActive? bgColorPressed : "transparent"
        }
    }

    /*onPressed: focus = true
    onReleased: focus = false*/

}
