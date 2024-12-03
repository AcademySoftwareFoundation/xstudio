// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xStudio 1.0

Item {

    height: XsStyleSheet.widgetStdHeight

    // divider line
    Rectangle {
        width: parent.width
        anchors.bottom: parent.bottom
        height: 1
        color: XsStyleSheet.widgetBgNormalColor
    }

    RowLayout {

        anchors.fill: parent
        spacing: 10

        XsLabel {
            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
            Layout.preferredWidth: row_widths[0]
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            text: hotkeyName
            horizontalAlignment: Text.AlignLeft
            onTextWidthChanged: {
                setRowMinWidth(textWidth, 0)
            }

        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: XsStyleSheet.widgetBgNormalColor
        }

        /*XsLabel {
            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
            Layout.preferredWidth: 0// row_widths[1]
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            text: hotkeyCategory
            horizontalAlignment: Text.AlignLeft
            visible: false
            onTextWidthChanged: {
                setRowMinWidth(textWidth, 1)
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: XsStyleSheet.widgetBgNormalColor
        }*/

        XsLabel {
            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
            Layout.preferredWidth: row_widths[2]
            Layout.bottomMargin: 4
            Layout.topMargin: 4
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            text: hotkeySequence
            horizontalAlignment: Text.AlignLeft
            onTextWidthChanged: {
                setRowMinWidth(textWidth, 2)
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: XsStyleSheet.widgetBgNormalColor
        }

        Item {

            Layout.preferredWidth: 20
            Layout.fillHeight: true

            XsImage {

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 5
                width: height
                height: parent.height -2
                source: "qrc:/icons/help.svg"
                imgOverlayColor: ma.containsMouse ? palette.highlight : XsStyleSheet.hintColor
                antialiasing: true
                smooth: true

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        tooltip.visible = true
                    }
                }

            }

            XsToolTip {
                id: tooltip
                text: hotkeyDescription
                width: Math.min(300, metricsDiv.width)
                visible: ma.containsMouse
            }

        }

    }
}
