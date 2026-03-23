// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0


Item{

    property real maxTextWidth: 0

    XsHotkeyReference {
        id: cycle_up_hotkey
        hotkeyName: "Cycle Image Layer / EXR Part (Up)"
    }

    XsHotkeyReference {
        id: cycle_down_hotkey
        hotkeyName: "Cycle Image Layer / EXR Part (Down)"
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 1

        XsText { id: textDiv
            Layout.margins: 10
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            
            text: "Select Image Layer (EXR group/part): "
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
        }

        XsListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            anchors.margins: 6
            model: currentPlayhead.image_stream_options
            delegate: listItem

            ScrollBar.vertical: XsScrollBar {
                visible: listView.height < listView.contentHeight
            }

        }

        XsText {
            Layout.margins: 10
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight*2
            
            text: "(Use hotkeys "+cycle_up_hotkey.sequence +"/"+ cycle_down_hotkey.sequence + " to cycle through the image layers.)"
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.italic: true
            wrapMode: Text.Wrap
        }

        XsSimpleButton {

            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            Layout.rightMargin: 10
            Layout.bottomMargin: 10

            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                close()
            }
        }
    }

    Component{
        id: listItem

        Item{
            width: listView.width
            height: XsStyleSheet.widgetStdHeight
            
            property var currentText: currentPlayhead.image_stream_options[index]
            property bool isChecked: currentText == currentPlayhead.current_image_stream

            MouseArea{ id: mArea
                hoverEnabled: true
                anchors.fill: parent
                
                onClicked: {
                    currentPlayhead.current_image_stream = currentText
                    //listView.currentIndex = index
                }
            }

            RowLayout{
                anchors.fill: parent

                Item{
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Rectangle{
                    Layout.preferredWidth: radioDiv.width + textDiv.width + 12
                    Layout.fillHeight: true
                    color: "transparent"
                    border.width: mArea.containsMouse? 1:0
                    border.color: XsStyleSheet.accentColor

                    RowLayout{
                        anchors.fill: parent
                        spacing: 3

                        Item{
                            Layout.minimumWidth: 2
                            Layout.maximumWidth: 2
                            Layout.fillHeight: true
                        }
                        Item{ id: radioDiv
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            
                            XsIcon {
                                anchors.fill: parent
                                source: isChecked ? 
                                    "qrc:/icons/radio_button_checked.svg" : 
                                    "qrc:/icons/radio_button_unchecked.svg"
                            }
                        }
                        
                        XsText { id: textDiv
                            Layout.minimumWidth: 100
                            Layout.preferredWidth: maxTextWidth
                            Layout.fillHeight: true
                            
                            text: currentPlayhead.image_stream_options[index]
                            horizontalAlignment: Text.AlignLeft
                            elide: Text.ElideRight
                            // color: isChecked? XsStyleSheet.accentColor : XsStyleSheet.primaryTextColor
                            
                            Component.onCompleted: {
                                if(maxTextWidth < textWidth) maxTextWidth = textWidth
                            }
                        }
                        Item{
                            Layout.fillWidth: true
                            Layout.minimumWidth: 2
                            Layout.maximumWidth: 2
                            Layout.fillHeight: true
                        }
                    }
                }
                
                Item{
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

            }

        }


    }
}
