// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xstudio.qml.viewport 1.0
import xStudio 1.0

import "./delegates"

XsWindow {
    id: dialog

    minimumWidth: 560
	height: 750
    title: "xSTUDIO Hotkeys"

    property var hoveredIndex: -1

    property int widthHint: 334

    property int groupColumns: Math.max(1, (width-60)/widthHint)
    property int wHint: 50

    ColumnLayout {

        id: rootLayout
        anchors.fill: parent
        anchors.margins: 10
        spacing: 20

        XsText {
            Layout.leftMargin: 10
            Layout.topMargin: 10
            text: qsTr("Find and customize hotkeys for all actions in xSTUDIO. Click on a hotkey entry to modify the key binding. Hover your pointer over the hotkey entry to see a description of the associated action.")
            font.pixelSize: XsStyleSheet.fontSize + 2
            font.italic: true
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignLeft
        }

        RowLayout {

            id: rlayout
            Layout.margins: 10
            spacing: 10

            ColumnLayout {

                spacing: 0
                Layout.alignment: Qt.AlignTop

                XsSearchButton{
                    id: searchBtn
                    Layout.preferredWidth: wHint
                    Layout.preferredHeight: 24
                    isExpanded: true
                    hint: qsTr("Filter ...")
                    onTextChanged: {
                        hotkeysModel.searchString = text
                    }
                }

                XsMenuDivider {
                    Layout.preferredWidth: wHint
                    text: qsTr("Jump To ...")
                }

                Repeater {
                    model: hotkeysModel.categories
                    XsSimpleButton {
                        Layout.preferredWidth: wHint
                        Layout.topMargin: 8
                        text: modelData
                        onIdealWidthChanged: {
                            if (idealWidth > wHint) {
                                wHint = idealWidth
                            }
                        }
                        onClicked: {
                            flickable.autoScrollToCategory(modelData)
                        }
                    }
                }

            }

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: XsStyleSheet.menuBorderColor
            }

            Flickable {
                id: flickable
                contentHeight: hotkeysListView.height
                Layout.fillHeight: true
                Layout.fillWidth: true

                clip: true
                ColumnLayout {

                    id: hotkeysListView
                    clip: true
                    spacing: 8
                    width: parent.width-16 // space for scrollbar

                    Repeater {
                        model: hotkeysModel
                        id: repeater
                        XsHotkeyGroup {
                            Layout.fillWidth: true
                            model: hotkeysModel
                            rootIndex: model.index(index, 0)
                            property var cat: hotkeyCategory
                            function autoScrollToCategory() {
                                autoScrollAnimator.to = y
                                autoScrollAnimator.running = true
                            }
                        }
                    }
                }

                ScrollBar.vertical: XsScrollBar {
                    width: 12
                    visible: flickable.height < hotkeysListView.height
                    policy: ScrollBar.AlwaysOn
                }

                PropertyAnimation{
                    id: autoScrollAnimator
                    target: flickable
                    properties: "contentY"
                    duration: 100
                }

                function autoScrollToCategory(category) {

                    for (var i=0; i<repeater.count; i++) {
                        var item = repeater.itemAt(i)
                        if (item.cat === category) {
                            item.autoScrollToCategory()
                        }
                    }
                }
          }
        }

        XsSimpleButton {

            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            Layout.rightMargin: 10
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.close()
            }
        }
    }

    Loader {
        id: setterLoader
    }

    Component {
        id: hotkeySetterComponent
        XsHotkeySetterDialog {

        }
    }

    function showHotkeySetter(hotkeyUUID) {

        hotkeysModel.testHotkeyID = hotkeyUUID
        setterLoader.sourceComponent = hotkeySetterComponent
        setterLoader.item.hotkeyUUID = hotkeyUUID
        setterLoader.item.visible = true
    }

}