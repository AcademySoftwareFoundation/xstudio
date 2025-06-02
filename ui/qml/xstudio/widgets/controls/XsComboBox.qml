// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
// import QtQuick.Templates as T

import xStudio 1.0

ComboBox { id: widget

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

    property alias placeholderText: textField.placeholderText
    property alias placeholderTextColor: textField.placeholderTextColor
    property alias defaultText: textField.placeholderText

    property real fontSize: XsStyleSheet.fontSize
    property var fontFamily: XsStyleSheet.fontFamily

    rightPadding: editable? (padding*2 + indicator.width): (padding)

    property bool textTruncated: textField.elidedText != textMetrics2.text
    property var unElidedTextWidth: textMetrics2.width

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
    TextField{ id: textField
        // placeholderText: defaultText

        text: widget.displayText// activeFocus && widget.editable ? widget.displayText : (textMetrics.elidedText ? textMetrics.elidedText : "")

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

        placeholderTextColor: textColorDisabled

        // TextMetrics { id: textMetrics
        //     font: textField.font
        //     text: widget.displayText
        //     elideWidth: textField.width - 8
        //     elide: Text.ElideRight
        // }

        TextMetrics { id: textMetrics2
            font: textField.font
            text: widget.displayText
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
    XsIcon{
        opacity: enabled? 1:0.3
        source: "qrc:///icons/arrow_drop_down.svg"
        width: 25//widget.width>=25 ? 25:widget.width
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
                z:2
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
            id: tt
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

            XsToolTip{
                id: toolTip
                text: tt.text
                visible: widget.highlightedIndex === index && tt.truncated
                x: 0 //#TODO: flex/pointer
            }
    
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