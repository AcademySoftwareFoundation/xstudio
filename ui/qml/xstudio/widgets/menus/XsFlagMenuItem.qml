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
    width: parent.width

    // Note .. menu item width needs to be set by the widest menu item in the
    // same menu. This creates a circular dependency .. the menu item width
    // depends on the widest item. If it is the widest item its width
    // depends on itself. There must be a better QML solution for this
    property real minWidth: indentSpacer.width + colourIndicator.width + label_metrics.width + margin*4

    property real leftIconSize: indentSpacer.width + colourIndicator.width

    property var label: name ? name : ""

    property var margin: 4 // don't need this ?

    property bool isActive: menuMouseArea.pressed

    property var parent_menu

    MouseArea{
        id: menuMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        onClicked: {
            menu_model.nodeActivated(
                menu_model_index,
                "menu",
                helpers.contextPanel(widget)
                )

            closeAll()    
        }
    }

    Rectangle {
        anchors.fill: parent
        border.color: palette.highlight
        border.width: XsStyleSheet.widgetBorderWidth
        color: isActive ? palette.highlight : "transparent"
        visible: menuMouseArea.containsMouse
    }

    RowLayout {

        anchors.fill: parent
        anchors.margins: margin
        spacing: 0

        Rectangle {

            id: colourIndicator
            width: XsStyleSheet.menuCheckboxSize
            height: XsStyleSheet.menuCheckboxSize
            radius: width/2
            color: user_data

        }

        Item {
            id: indentSpacer
            width: 3
        }

        XsText {
            id: labelDiv
            text: label ? label : "Unknown" //+ (sub_menu && !is_in_bar ? "   >>" : "")
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            color: palette.text
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
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