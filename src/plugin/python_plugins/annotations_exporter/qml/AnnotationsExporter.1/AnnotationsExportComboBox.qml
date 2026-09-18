// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

RowLayout {

    property alias attr_title: combo.attr_title
    property alias attr_model_name: combo.attr_model_name
    property alias currentText: combo.currentText
    property alias currentIndex: combo.currentIndex

    function override(text) {
        combo.currentIndex = combo.indexOfValue(text)
    }

    XsAttrComboBox { 

        id: combo
        Layout.preferredHeight: widgetHeight
        Layout.preferredWidth: 324
        attr_model_name: "annotation_export_attrs"
    }

    Item {

        Layout.preferredWidth: 20
        Layout.fillHeight: true

        XsIcon {

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 5
            width: height
            height: parent.height -2
            source: "qrc:/icons/help.svg"
            imgOverlayColor: ma.containsMouse ? XsStyleSheet.accentColor : XsStyleSheet.hintColor
            antialiasing: true
            smooth: true
            visible: tooltip.value != undefined

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
            }

        }

        XsToolTip {
            text: tooltip.value
            maxWidth: combo.width*1.5
            visible: ma.containsMouse
        }

    }

    XsAttributeValue {
        id: tooltip
        attributeTitle: attr_title
        role: "tooltip"
        model: combo.model_data
    }

}
