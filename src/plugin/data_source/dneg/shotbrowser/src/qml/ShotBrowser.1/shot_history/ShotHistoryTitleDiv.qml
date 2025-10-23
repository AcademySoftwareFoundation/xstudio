// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

RowLayout {id: titleDiv

    property int titleButtonCount: 4
    property real titleButtonSpacing: 1
    property real titleButtonHeight: XsStyleSheet.widgetStdHeight+4

    XsPrimaryButton{ id: updateScopeBtn
        Layout.preferredWidth: 40
        Layout.fillHeight: true

        imgSrc: isPanelEnabled && !isPaused ? "qrc:///shotbrowser_icons/lock_open.svg" : "qrc:///shotbrowser_icons/lock.svg"
        // text: isPanelEnabled? "ON" : "OFF"
        isActive: !isPanelEnabled || isPaused
        onClicked: isPanelEnabled = !isPanelEnabled
    }

    ColumnLayout{ id: col
        Layout.fillWidth: true
        Layout.fillHeight: true

        spacing: titleButtonSpacing

        RowLayout{
            Layout.fillWidth: true
            Layout.preferredHeight: titleButtonHeight
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
                    model: ShotBrowserEngine.presetsModel.shotHistoryScope

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
            spacing: 0

            XsSearchButton{ id: filterBtn
                Layout.fillWidth: true
                Layout.fillHeight: true
                autoDefocus: true
                isExpanded: true
                hint: "Filter"
                buttonWidth: scopeTxt.width
                enabled: isPanelEnabled && !isPaused

                onTextChanged: nameFilter = text
                onEditingCompleted: focus = false

                Connections {
                    target: panel
                    function onNameFilterChanged() {
                        filterBtn.text = nameFilter
                    }
                }
            }

            XsComboBoxEditable{ id: filterSentTo
                Layout.fillHeight: true
                Layout.minimumWidth: titleButtonHeight * 2
                Layout.preferredWidth: titleButtonHeight * 3.5

                enabled: isPanelEnabled && !isPaused

                model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Sent To") : []
                currentIndex: -1
                textRole: "nameRole"
                placeholderText: "Sent To"

                onModelChanged: currentIndex = -1

                onCurrentIndexChanged: {
                    if(currentIndex==-1)
                        sentTo = ""
                }

                onAccepted: {
                    sentTo = model.get(model.index(currentIndex,0), "nameRole")
                }

                onActivated: sentTo = model.get(model.index(currentIndex,0), "nameRole")

                Connections {
                    target: panel
                    function onSentToChanged() {
                        filterSentTo.currentIndex = filterSentTo.find(sentTo)
                    }
                }
            }
        }
    }
}