// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0

Flickable {

    id: flick
    clip: true
    contentHeight: contentItem.childrenRect.height

    ScrollBar.vertical: XsScrollBar {
        id: scrollbar
        width: 10
        visible: flick.height < theLayout.height
    }

    XsModuleData {
        id: hdr_values
        modelDataName: "Decklink HDR Values"
    }
    
    XsModuleData {
        id: hdr_settings
        modelDataName: "Decklink HDR Settings"
    }

    XsAttributeValue {
        id: __hdrPresetsOptions
        attributeTitle: "HDR Presets"
        model: hdr_settings
        role: "combo_box_options"
    }
    XsAttributeValue {
        id: __hdrPresetsValue
        attributeTitle: "HDR Presets"
        model: hdr_settings
    }

    property alias preset_options: __hdrPresetsOptions.value
    property alias preset_value: __hdrPresetsValue.value

    ColumnLayout {

        id: theLayout
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top

        Item {
            Layout.preferredHeight: 10
        }

        GridLayout {

            id: grid
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            XsText {
                Layout.alignment: Qt.AlignRight
                text: "HDR Mode"
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }

            DecklinkMultichoiceSetting {
        
                Layout.preferredHeight: 24
                Layout.preferredWidth: 100
                attrs_model: hdr_settings
                attr_name: "HDR Mode"
    
            }    

            XsText {
                Layout.alignment: Qt.AlignRight
                text: "Colour Space"
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }

            DecklinkMultichoiceSetting {
        
                Layout.preferredHeight: 24
                Layout.preferredWidth: 100
                attrs_model: hdr_settings
                attr_name: "Colour Space"
    
            }    

        }

        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                Layout.preferredHeight: 1
                Layout.fillWidth: true
            }
            XsText {
                text: "Colourimetry"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                Layout.preferredHeight: 1
                Layout.fillWidth: true
            }
        }

        GridLayout {

            columns: 4
            rowSpacing: 10
            columnSpacing: 10

            Repeater {

                model: hdr_values
        
                XsText {
                    Layout.alignment: Qt.AlignRight
                    text: title
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.row: index/2
                    Layout.column: (index%2)*2

                }
            }

            Repeater {
                model: hdr_values

                XsTextField {

                    Layout.alignment: Qt.AlignLeft
                    Layout.preferredHeight: 24                
                    Layout.preferredWidth: 80                
                    Layout.row: index/2
                    Layout.column: (index%2)*2 + 1
                                
                    text: Number.parseFloat(valFollower).toFixed(4).replace(/^(\d+)(?:\.0+|(\.\d*?)0+)$/, "$1$2")
                    width: 24
                    selectByMouse: true
    
                    property var valFollower: value
                    onValFollowerChanged: {
                        text = Number.parseFloat(valFollower).toFixed(4).replace(/^(\d+)(?:\.0+|(\.\d*?)0+)$/, "$1$2")
                    }
                    onEditingFinished: {
                        focus = false
                        value = parseFloat(text)
                    }
                }                
            }

            XsText {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.row: 9
                Layout.column: 0
                Layout.preferredHeight: 24                
                text: "Presets"
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }

            RowLayout {

                Layout.row: 9
                Layout.column: 1
                Layout.columnSpan: 3

                Repeater {
                    model: preset_options
                    XsSimpleButton {
                        text: preset_options[index]
                        Layout.preferredWidth: 70
                        Layout.fillHeight: true
                        DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                        onClicked: {
                            preset_value = preset_options[index]
                        }                            
                    }
                }

            }
        }
    }
}