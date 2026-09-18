// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts


import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

RowLayout {

    readonly property real titleButtonSpacing: 1
    property real titleButtonHeight: XsStyleSheet.widgetStdHeight+4

    XsPrimaryButton {
        Layout.preferredWidth: 40
        Layout.fillHeight: true

        imgSrc: isPanelEnabled && !isPaused ? "qrc:///shotbrowser_icons/lock_open.svg" : "qrc:///shotbrowser_icons/lock.svg"
        // text: isPanelEnabled? "ON" : "OFF"
        isActive: !isPanelEnabled || isPaused
        onClicked: isPanelEnabled = !isPanelEnabled
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        spacing: titleButtonSpacing

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            // width: parent.width
            // height: titleButtonHeight
            spacing: 0

            XsText{ id: scopeTxt
                Layout.preferredWidth: (textWidth + panelPadding*3)
                Layout.fillHeight: true
                text: "Scope: "
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                enabled: isPanelEnabled && !isPaused
                spacing: titleButtonSpacing

                Repeater {
                    model: ShotBrowserEngine.presetsModel.noteHistoryScope

                    XsPrimaryButton{
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: ShotBrowserEngine.presetsModel.get(modelData, "nameRole")

                        isActive: activeScopeIndex == modelData
                        property bool isRunning: queryRunning && isActive

                        onClicked: {
                            activateScope(modelData)
                        }
                        XsBusyIndicator{
                            x: 4
                            width: height
                            height: parent.height
                            running: visible
                            visible: isRunning
                            scale: 0.5
                        }
                    }
                }
            }
        }

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
            // width: parent.width
            // height: titleButtonHeight
            spacing: 0

            XsText{
                Layout.preferredWidth: scopeTxt.width
                Layout.fillHeight: true
                text: "Type: "
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                enabled: isPanelEnabled && !isPaused
                spacing: titleButtonSpacing

                Repeater {
                    model: ShotBrowserEngine.presetsModel.noteHistoryType

                    XsPrimaryButton{
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: ShotBrowserEngine.presetsModel.get(modelData, "nameRole")

                        isActive: activeTypeIndex == modelData
                        property bool isRunning: queryRunning && isActive


                        onClicked: {
                            activateType(modelData)
                        }
                        XsBusyIndicator{
                            x: 4
                            width: height
                            height: parent.height
                            running: visible
                            visible: isRunning
                            scale: 0.5
                        }
                    }
                }
            }
        }
    }
}