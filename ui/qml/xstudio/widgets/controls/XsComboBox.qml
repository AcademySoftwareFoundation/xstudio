// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T

import xStudio 1.0

T.ComboBox { id: widget

    editable: false
    clip: true

    property bool wasEditable: false
    property bool isActive: false

    property alias popupOptions: popupOptions
    property alias textField: textField

    property color bgColorEditable: "light grey"
    property color bgColorActive: palette.highlight
    property color forcedBgColorNormal: bgColorNormal
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color bgColorPressed: palette.highlight
    property color textColorSelected: palette.text
    property color textColorNormal: palette.text
    property color textColorDisabled: XsStyleSheet.secondaryTextColor

    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property real borderRadius: 0
    property real framePadding: 6

    property real fontSize: XsStyleSheet.fontSize
    property var fontFamily: XsStyleSheet.fontFamily

    property string tooltip_text: ""
    ToolTip.delay: 500
    ToolTip.visible: hovered && tooltip_text != ""
    ToolTip.text: tooltip_text


    rightPadding: editable? (padding*2 + indicator.width): (padding)

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    wheelEnabled: false
    selectTextByMouse: true
    // autoScroll: editable

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: widget.down || widget.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: "transparent"

        XsGradientRectangle{
            anchors.fill: parent

            flatColor: topColor
            topColor: widget.down || isActive? bgColorPressed: XsStyleSheet.controlColour
            bottomColor: widget.down || isActive? bgColorPressed: forcedBgColorNormal
        }

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
    }

    contentItem:
    T.TextField{ id: textField

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
        // onAccepted: focus = false

        onEditingFinished: accepted()

        enabled: widget.editable
        autoScroll: widget.editable
        readOnly: widget.down
        inputMethodHints: widget.inputMethodHints
        validator: widget.validator
        selectByMouse: widget.selectTextByMouse

        selectionColor: widget.palette.highlight
        selectedTextColor: textColorSelected

        font.pixelSize: fontSize
        font.family: XsStyleSheet.fontFamily
        color: widget.enabled && currentIndex!=-1? textColorNormal: textColorDisabled
        width: widget.width
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        TextMetrics { id: textMetrics
            font: textField.font
            text: widget.displayText
            elideWidth: textField.width - 8
            elide: Text.ElideRight
        }

        background:
        Rectangle{
            radius: borderRadius
            color: widget.editable ? widget.popup.opened || widget.activeFocus? Qt.darker(bgColorActive, 1.5): widget.hovered?Qt.darker(bgColorEditable, 1.2):Qt.darker(bgColorEditable, 1.5): "transparent";
            opacity: widget.enabled? 0.7 : 0.3
            scale: 0.99
            z:-1
        }
    }

    indicator:
    XsImage{
        opacity: enabled? 1:0.3
        source: "qrc:///icons/arrow_drop_down.svg"
        sourceSize.width: 25
        sourceSize.height: 20
        width: widget.width>=25 ? 25:widget.width
        height: 20
        anchors.right: widget.right
        anchors.verticalCenter: widget.verticalCenter
        clip: true
    }

    popup:
    XsPopup{
        id: popupOptions
        width: widget.width
        implicitHeight: contentItem.implicitHeight + (topPadding + bottomPadding)
        padding: 1
        y: widget.height

        contentItem:
        ListView { id: listView
            clip: true
            implicitHeight: contentHeight
            model: widget.popup.visible ? widget.delegateModel: null
            currentIndex: widget.highlightedIndex
            snapMode: ListView.SnapToItem

            ScrollBar.vertical:
            XsScrollBar {id: control
                padding: 2
                policy: listView.model && (listView.height< (widget.height*listView.model.count))? ScrollBar.AlwaysOn: ScrollBar.AlwaysOff
            }
        }

        onOpened: {
            widget.wasEditable = widget.editable
            widget.editable = false
            forceActiveFocus()
            focus = true
        }

        onClosed: {
            widget.focus = false
            focus = false
            widget.editable = widget.wasEditable

            // ensure active focus is passed back to viewport
            // sessionWidget.playerWidget.viewport.forceActiveFocus() //#TODO
        }
    }

    delegate: //Popup-ListView's delegate
    ItemDelegate {
        id: itemDelegate
        width: widget.width
        height: widget.height
        padding: 0

        contentItem:
        XsText {
            text: widget.textRole ? (Array.isArray(widget.model) ? modelData[widget.textRole]: model[widget.textRole]): modelData
            font.pixelSize: fontSize
            font.family: fontFamily
            font.weight: widget.currentIndex === index? Font.ExtraBold: Font.Normal
            color: textColorNormal
            elide: Text.ElideRight
            width: widget.width
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: itemDelegate.verticalCenter
        }

        highlighted: widget.highlightedIndex === index
        hoverEnabled: widget.hoverEnabled
        palette.text: widget.palette.text
        palette.highlightedText: widget.palette.highlightedText

        background:
        Rectangle{
            radius: borderRadius
            border.width: borderWidth
            border.color: widget.highlightedIndex === index? bgColorActive: "transparent"
            color: "transparent"
            width: widget.width
            height: widget.height
        }
    }

}