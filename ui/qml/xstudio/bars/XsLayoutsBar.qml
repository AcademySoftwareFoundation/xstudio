// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {

    property int currentIndex: 0
    border.color: Qt.darker(XsStyle.mainBackground,1.2)
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(XsStyle.mainBackground,1.2) }
        GradientStop { position: 1.0; color: XsStyle.mainBackground }
    }
    property var current_layout: sessionWidget.layout_name

    ListModel{

        id: layoutsModel

        // ListElement{
        //     name: "Browse"
        //     layout_name: "browse_layout"
        // }
        ListElement{
            name: "Edit"
            layout_name: "edit_layout"
        }
        ListElement{
            name: "Review"
            layout_name: "review_layout"
        }
        ListElement{
            name: "Present"
            layout_name: "presentation_layout"
        }
    }

    RowLayout {

        id: buttons_layout
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 4
        anchors.rightMargin: 12

        spacing: 5

        Repeater {

            id: repeater
            model: layoutsModel

            Rectangle {

                id: plus_button
                width: text_metrics.width+10
                color: "transparent"
                Layout.fillHeight: true
                property var hovered: mouse_area.containsMouse
                property bool is_active: current_layout === layout_name

                Rectangle {
                    anchors.fill: parent
                    color: "white"
                    opacity: 0.15
                    visible: hovered
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: name
                    verticalAlignment: Text.AlignVCenter
                    color: is_active ? XsStyle.controlColor : XsStyle.controlTitleColor
                    font.family: XsStyle.controlTitleFontFamily
                    font.pixelSize: XsStyle.layoutsBarFontSize

                }

                TextMetrics {
                    id: text_metrics
                    font: label.font
                    text: label.text
                }

                MouseArea {
                    id: mouse_area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        sessionWidget.layout_name = layout_name
                    }
                }


            }
        }
    }

}
