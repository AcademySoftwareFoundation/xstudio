import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.3

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: widget
    height: XsStyleSheet.menuHeight

    // Note .. menu item width needs to be set by the widest menu item in the
    // same menu. This creates a circular dependency .. the menu item width
    // depends on the widest item. If it is the widest item its width
    // depends on itself. There must be a better QML solution for this
    property real minWidth: gap.width + indentSpacer.width + iconDiv.width + label_metrics.width + gap.width + margin*4

    property var menu_item_text: ""
    property real indent: 0
    property var margin: 4
    property var menu_icon
    property alias icon: iconDiv.source

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed

    property real labelPadding: XsStyleSheet.menuLabelPaddingLR/2
    property color hotKeyColor: XsStyleSheet.secondaryTextColor
    property real borderWidth: XsStyleSheet.widgetBorderWidth

    // this darkens the text, icons etc if disabled
    opacity: enabled ? 1 : 0.5

    signal clicked()

    MouseArea
    {

        id: menuMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        onClicked: {
            widget.clicked()
        }
    }

    Rectangle {
        id: bgHighlightDiv
        implicitWidth: parent.width
        implicitHeight: widget.is_in_bar? parent.height- (borderWidth*2) : parent.height
        anchors.verticalCenter: parent.verticalCenter
        border.color: palette.highlight
        border.width: borderWidth
        color: isActive ? palette.highlight : "transparent"
        visible: widget.isHovered

    }

    RowLayout {

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: margin
        spacing: 0

        XsImage {

            id: iconDiv
            visible: source != ""
            source: menu_icon ? menu_icon : ""
            Layout.preferredWidth: source != "" ? height : 0
            //height: parent.height
            Layout.alignment: Qt.AlignVCenter
            Layout.fillHeight: true
            layer {
                enabled: true
                effect: ColorOverlay { color: hotKeyColor }
            }

        }

        Item {
            id: indentSpacer
            Layout.preferredWidth: iconDiv.width ? 3 : indent
        }

        XsText {

            id: labelDiv
            text: menu_item_text
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            color: palette.text
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignVCenter

        }

        TextMetrics {
            id:     label_metrics
            font:   labelDiv.font
            text:   labelDiv.text
        }

        Item {
            Layout.fillWidth: true
        }

    }

}