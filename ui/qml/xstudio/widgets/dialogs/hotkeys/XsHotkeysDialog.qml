// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xstudio.qml.viewport 1.0
import xStudio 1.0

import "./delegates"

XsWindow {
    id: dialog

    width: 700
    minimumWidth: 600
    height: 500
    minimumHeight: 350

    property var row_widths: [180, 140, 100]
    function setRowMinWidth(w, i) {
        if (w > row_widths[i]) {
            var r = row_widths
            r[i] = w
            row_widths = r
        }
    }

    property string searchFilter: ""

    title: "xSTUDIO Hotkey Editor"

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Tab bar for categories
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
                        border.color: hovered ? palette.highlight : "transparent"
                        color: tabBar.currentIndex == index ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                    }
                }
            }

            onCurrentIndexChanged: {
                hotkeysModel.currentCategory = hotkeysModel.categories[currentIndex]
            }
        }

        // Search bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Qt.darker(palette.base, 1.2)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                XsText {
                    text: "Search:"
                    Layout.alignment: Qt.AlignVCenter
                }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    placeholderText: "Filter hotkeys..."
                    color: palette.text
                    font.pixelSize: XsStyleSheet.fontSize
                    background: Rectangle {
                        color: palette.base
                        border.color: searchField.activeFocus ? palette.highlight : XsStyleSheet.widgetBgNormalColor
                        radius: 2
                    }
                    onTextChanged: {
                        dialog.searchFilter = text.toLowerCase()
                    }
                }
            }
        }

        // Column header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: XsStyleSheet.panelTitleBarColor

            RowLayout {
                anchors.fill: parent
                spacing: 0

                XsText {
                    Layout.preferredWidth: row_widths[0] + 20
                    Layout.leftMargin: 10
                    text: "Action"
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    color: XsStyleSheet.widgetBgNormalColor
                }

                XsText {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    text: "Shortcut"
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                }
            }
        }

        // Hotkey list
        XsListView {
            id: hotkeyListView

            Layout.fillWidth: true
            Layout.fillHeight: true

            model: hotkeysModel
            clip: true

            ScrollBar.vertical: XsScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: XsHotkeyEditorDelegate {
                searchFilter: dialog.searchFilter
            }
        }

        // Bottom toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: palette.base

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                XsSimpleButton {
                    text: qsTr("Reset All")
                    width: XsStyleSheet.primaryButtonStdWidth * 2
                    onClicked: {
                        hotkeysModel.resetAllHotkeys()
                    }
                }

                Item { Layout.fillWidth: true }

                XsSimpleButton {
                    text: qsTr("Close")
                    width: XsStyleSheet.primaryButtonStdWidth * 2
                    onClicked: {
                        dialog.close()
                    }
                }
            }
        }
    }
}
