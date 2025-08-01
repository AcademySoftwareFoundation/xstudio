// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T

import xStudio 1.1

T.ComboBox { id: widget

    property color bgColorEditable: "light grey"
    property color bgColorActive: palette.highlight
    property color bgColorNormal: palette.base

    property color textColorActive: palette.text
    property color textColorNormal: "light grey"
    property color textColorDisabled: Qt.darker(textColorNormal, 1.6)
    property color indicatorColorNormal: "light grey"

    property color borderColor: XsStyle.menuBorderColor
    property real borderWidth: 1
    property real borderRadius: 6
    property real framePadding: 6

    property real fontSize: XsStyle.menuFontSize
    property int displayItemCount: 10
    property bool wasEditable: false
    property alias popupOptions: popupOptions
    property bool downArrowVisible: true

    property string tooltip_text: ""

    ToolTip.delay: 500
    ToolTip.visible: hovered && tooltip_text != ""
    ToolTip.text: tooltip_text

    onHoveredChanged: {
        downArrow.requestPaint()
    }

    rightPadding: editable? (padding + indicator.width*2 + spacing): (padding)

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    wheelEnabled: false
    selectTextByMouse: true

    background:
    Rectangle{
        implicitWidth: 100
        implicitHeight: 22
        radius: borderRadius
        color: widget.popup.opened ? Qt.darker(bgColorActive, 2.75): bgColorNormal
        border.color: widget.hovered || widget.popup.opened? bgColorActive: bgColorNormal
        border.width: widget.pressed? 2: 1
    }

    contentItem:
    T.TextField{ id: textField
        // rightPadding: indicator.width
        // text: widget.editable ? widget.editText: widget.displayText
        text: activeFocus && widget.editable ? widget.displayText: textMetrics.elidedText
        
        onFocusChanged: {
            if(focus) {
                selectAll()
                forceActiveFocus()
            }
            else{
                deselect()
            }
        }
        onReleased: focus = false
        onAccepted: focus = false

        enabled: widget.editable
        autoScroll: widget.editable
        readOnly: widget.down
        inputMethodHints: widget.inputMethodHints
        validator: widget.validator
        selectByMouse: widget.selectTextByMouse

        selectionColor: widget.palette.highlight
        selectedTextColor: textColorActive //widget.palette.highlightedText

        font.pixelSize: fontSize
        font.family: XsStyle.fontFamily
        color: widget.enabled? widget.hovered || widget.activeFocus? textColorActive: textColorNormal: textColorDisabled
        width: widget.width //- indicatorMArea.width
        horizontalAlignment: Text.AlignHCenter
        height: widget.height
        verticalAlignment: Text.AlignVCenter
        topPadding: (widget.height-textMetrics.height)/4

        TextMetrics { id: textMetrics
            font: textField.font
            text: widget.displayText //widget.displayText
            elideWidth: textField.width - 8
            elide: Text.ElideRight
        }

        background:
        Rectangle{
            radius: borderRadius
            color: widget.editable ? widget.popup.opened || widget.activeFocus? Qt.darker(bgColorActive, 1.5): widget.hovered?Qt.darker(bgColorEditable, 1.2):Qt.darker(bgColorEditable, 1.5): "transparent";
            opacity: widget.enabled? 0.7 : 0.3
            // width: widget.width
            // height: widget.height
            scale: 0.99
            z:-1
        }

    }

    indicator:
    Canvas {
        id: downArrow
        x: editable? (widget.width - width - widget.rightPadding/3): (widget.width - width*1.5 - widget.rightPadding/3)
        y: widget.topPadding + (widget.availableHeight - height) / 2
        z: 10
        width: 12
        height: 8
        contextType: "2d"
        opacity: widget.enabled ? 1: 0.3
        visible: downArrowVisible

        Connections {
            target: widget
            function onPressedChanged()
            {
                downArrow.requestPaint();
            }
        }

        onPaint: {
            if(context !== undefined) {
                context.reset();
                context.moveTo(0, 0);
                context.lineTo(width, 0);
                context.lineTo(width / 2, height);
                context.closePath();
                context.lineWidth= borderWidth
                context.strokeStyle = (!editable && widget.hovered) || (editable && indicatorMArea.containsMouse) || widget.popup.opened? bgColorActive: indicatorColorNormal
                context.stroke();
            }
        }

        MouseArea{ id: indicatorMArea
            anchors.centerIn: parent

            width: widget.width - downArrow.x +4
            height: widget.availableHeight
            hoverEnabled: true
            propagateComposedEvents: true
            onHoveredChanged: {
                downArrow.requestPaint()
            }
            onClicked: {
                if(popupOptions.opened) popupOptions.close()
                else popupOptions.open()
            }
        }
    }

    popup:
    T.Popup{
        id: popupOptions
        width: widget.width
        height: contentItem.implicitHeight
        y: widget.height
        padding: 0

        contentItem:
        ListView { id: listView
            clip: true
            implicitHeight: contentHeight>widget.height*displayItemCount? widget.height*displayItemCount: contentHeight
            model: widget.popup.visible ? widget.delegateModel: null
            currentIndex: widget.highlightedIndex
            snapMode: ListView.SnapToItem

            ScrollBar.vertical:
            XsScrollBar {id: control
                padding: 2
                policy: listView.model && (listView.height< (widget.height*listView.model.count))? ScrollBar.AlwaysOn: ScrollBar.AlwaysOff
            }
        }

        background:
        Rectangle{
            radius: borderRadius
            color: bgColorNormal
            border.color: borderColor
            border.width: 1
        }
        onOpened: {
            widget.wasEditable = widget.editable
            widget.editable = false
            forceActiveFocus()
            focus = true
            downArrow.requestPaint()
        }
        onClosed: {
            widget.focus = false
            focus = false
            downArrow.requestPaint()
            widget.editable = widget.wasEditable
            // ensure active focus is passed back to viewport
            sessionWidget.playerWidget.viewport.forceActiveFocus()
        }
    }

    delegate: //Popup-ListView's delegate
    ItemDelegate {
        id: itemDelegate
        width: widget.width
        height: widget.height
        padding: 0

        contentItem:
        Text{
            text: widget.textRole ? (Array.isArray(widget.model) ? modelData[widget.textRole]: model[widget.textRole]): modelData
            font.pixelSize: fontSize
            font.family: XsStyle.fontFamily
            font.weight: widget.currentIndex === index? Font.ExtraBold: Font.Normal
            color: widget.highlightedIndex === index || widget.currentIndex === index? textColorActive: textColorNormal
            elide: Text.ElideRight
            width: widget.width
            // height: widget.height
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        highlighted: widget.highlightedIndex === index
        hoverEnabled: widget.hoverEnabled
        palette.text: widget.palette.text
        palette.highlightedText: widget.palette.highlightedText

        background:
        Rectangle{
            radius: borderRadius
            color: widget.highlightedIndex === index? bgColorActive: "transparent"
            width: widget.width
            height: widget.height
        }
    }

}