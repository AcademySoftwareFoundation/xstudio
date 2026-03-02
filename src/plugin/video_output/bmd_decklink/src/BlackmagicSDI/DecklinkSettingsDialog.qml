// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0

XsWindow {

	id: bmd_settings_dialog
	minimumWidth: 500
	height: 500
    title: "Blackmagic Designs Decklink Output"
    property var widgetWidth: 150

    XsAttributeValue {
        id: __track_zoom
        attributeTitle: "Track Viewport"
        model: decklink_settings
    }
    property alias track_zoom: __track_zoom.value

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 20
        spacing: 0

        RowLayout {

            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            Image {
                id: decklink_image
                source: "qrc:/bmd_icons/decklink_image.png"
                Layout.maximumWidth: 100
                Layout.maximumHeight: 100
            }

            XsText {
                text: "Blackmagic Designs\nDecklink SDI Output"
                font.pixelSize: 20
                lineHeight: 1.2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter // Text.AlignLeft // Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        /*Rectangle {
            height: 1
            Layout.fillWidth: true
            Layout.margins: 8
            color: XsStyleSheet.menuDividerColor
        }*/

        Item {
            Layout.preferredHeight: 20
        }

        TabBar {
            id: tabBar

            Layout.fillWidth: true
            background: Rectangle {
                color: "transparent"
            }

            TabButton {

                id: but
                width: implicitWidth
                hoverEnabled: true

                contentItem: XsText {
                    text: "Main Settings"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: tabBar.currentIndex == 0
                }

                background: Rectangle {
                    border.color: but.hovered? XsStyleSheet.accentColor : "transparent"
                    color: tabBar.currentIndex == 0 ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                }
            }

            TabButton {

                id: but2
                width: implicitWidth
                hoverEnabled: true

                contentItem: XsText {
                    text: "HDR Settings"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: tabBar.currentIndex == 1
                }

                background: Rectangle {
                    border.color: but2.hovered? palette.highlight : "transparent"
                    color: tabBar.currentIndex == 1 ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                }
            }

        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: XsStyleSheet.menuDividerColor
        }

        Item {
            Layout.preferredHeight: 20
        }

        StackLayout {

            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            DecklinkMainSettings {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            DecklinkHDRSettings {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight|Qt.AlignBottom
            Layout.topMargin: 20
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                bmd_settings_dialog.close()
            }
        }

    }



}