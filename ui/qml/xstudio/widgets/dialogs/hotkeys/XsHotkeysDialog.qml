// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.viewport 1.0
import xStudio 1.0

import "./delegates"

XsWindow {
    id: dialog

	width: 550
	minimumWidth: 550
	height: 250
	// minimumHeight: 250
    property var row_widths: [100, 100, 100]
    function setRowMinWidth(w, i) {
        if (w > row_widths[i]) {
            var r = row_widths
            r[i] = w
            row_widths = r
        }
    }

    title: "xSTUDIO Hotkeys"

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 0
        spacing: 10

        TabBar {

            id: tabBar

            Layout.fillWidth: true
            background: Rectangle {
                color: palette.base
            }

            Repeater {

                model: hotkeysModel.categories

                TabButton {

                    width: implicitWidth

                    contentItem: XsText {
                        text: hotkeysModel.categories[index]
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: tabBar.currentIndex == index
                    }

                    background: Rectangle {
                        border.color: hovered? palette.highlight : "transparent"
                        color: tabBar.currentIndex == index ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                    }
                }

            }

            onCurrentIndexChanged: {
                hotkeysModel.currentCategory = hotkeysModel.categories[currentIndex]
            }

        }

        XsListView {

            Layout.fillWidth: true
            Layout.fillHeight: true

            model: hotkeysModel
            delegate: XsHotkeyDetails {}

        }

        XsSimpleButton {

            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            Layout.rightMargin: 10
            Layout.bottomMargin: 10
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.close()
            }
        }
    }

}