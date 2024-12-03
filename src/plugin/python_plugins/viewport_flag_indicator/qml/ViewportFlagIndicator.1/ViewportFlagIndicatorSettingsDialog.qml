// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.models 1.0

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xStudio 1.0

// This is an adapted version of XsAttributesPanel, allowing us to add a dynamic
// list of masks that the user can enable/disable

XsWindow {

	id: dialog
	width: 500
	minimumHeight: maskOptions.length*25 + 300
    title: "DNEG Pipeline Mask Settings"
    property real cwidth: 50

    XsModuleData {
        id: attribute_set
        modelDataName: "dnmask_settings"
    }

    XsModuleData {
        id: mask_options_data
        modelDataName: "dnmask_options"
    }

    XsModuleData {
        id: mask_other_data
        modelDataName: "dnmask_other"
    }

    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    XsAttributeValue {
        id: __current_show
        attributeTitle: "dnMask Current Show"
        model: mask_other_data
    }
    property alias currentShow: __current_show.value


    // make an alias so the mask options are accessible as an array
    property alias maskOptions: mask_options_data

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            spacing: 10
            Layout.fillWidth: true
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: XsStyleSheet.menuBorderColor
            }
            XsText {
                text: "Active Masks for " + currentShow
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: XsStyleSheet.menuBorderColor
            }
        }

        RowLayout {

            Layout.fillWidth: true
            Layout.margins: 10

            ColumnLayout {

                Layout.margins: 5

                Repeater {

                    // N.B the 'combo_box_options' attribute is propagated via the
                    // XsToolBox which instantiates the BasicViewportMaskButton
                    model: maskOptions

                    Rectangle {

                        height: 20
                        color: "transparent"
                        Layout.preferredWidth: cwidth
                        Layout.alignment: Qt.AlignRight

                        XsText {
                            id: thetext
                            anchors.fill: parent
                            text: title ? title : ""
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            font.strikeout: user_data == undefined
                        }

                        TextMetrics {
                            id:     label_metrics
                            font:   thetext.font
                            text:   thetext.text
                        }
                    }

                }
            }

            ColumnLayout {

                Layout.fillWidth: true
                Layout.margins: 5

                Repeater {

                    model: maskOptions
                    y: 5
                    delegate: Rectangle {
                        color: "transparent"
                        height: 20
                        Layout.fillWidth: true
                        XsBoolAttributeCheckBox {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.topMargin: 2
                            anchors.bottomMargin: 2
                        }
                    }

                }
            }
        }

        XsText {
            text: "Masks that are struck out have not been set-up for the current on-screen image format."
            Layout.alignment: Qt.AlignHCenter
            font.italic: true
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            spacing: 10
            Layout.fillWidth: true
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: XsStyleSheet.menuBorderColor
            }
            XsText {
                text: "Mask Settings"
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: XsStyleSheet.menuBorderColor
            }
        }

        RowLayout {

            id: settings_items
            Layout.fillWidth: true
            Layout.margins: 10

            ColumnLayout {

                Layout.margins: 5

                Repeater {

                    // N.B the 'combo_box_options' attribute is propagated via the
                    // XsToolBox which instantiates the BasicViewportMaskButton
                    model: attribute_set
                    id: r1

                    Rectangle {

                        height: 20
                        color: "transparent"
                        width: label_metrics.width
                        onWidthChanged: setCWidth(width)
                        Layout.alignment: Qt.AlignRight

                        XsText {
                            id: thetext
                            anchors.fill: parent
                            text: title ? title : ""
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            y: index*20
                        }

                        TextMetrics {
                            id:     label_metrics
                            font:   thetext.font
                            text:   thetext.text
                        }
                    }

                }
            }

            ColumnLayout {

                Layout.fillWidth: true
                Layout.margins: 5

                Repeater {

                    model: attribute_set
                    y: 5
                    delegate: chooser

                    DelegateChooser {
                        id: chooser
                        role: "type"

                        DelegateChoice {
                            roleValue: "FloatScrubber";
                            XsFloatAttributeSlider {
                                height: 20
                                Layout.fillWidth: true
                            }
                        }

                        DelegateChoice {
                            roleValue: "IntegerValue";
                            XsIntAttributeSlider {
                                height: 20
                                Layout.fillWidth: true
                            }
                        }

                        DelegateChoice {
                            roleValue: "ColourAttribute";
                            XsColourChooser {
                                height: 20
                                Layout.fillWidth: true
                            }
                        }


                        DelegateChoice {
                            roleValue: "OnOffToggle";
                            Rectangle {
                                color: "transparent"
                                height: 20
                                Layout.fillWidth: true
                                XsBoolAttributeCheckBox {
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.topMargin: 2
                                    anchors.bottomMargin: 2
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: "ComboBox";
                            Rectangle {
                                height: 20
                                Layout.fillWidth: true
                                color: "transparent"
                                XsComboBox {
                                    model: combo_box_options
                                    anchors.fill: parent
                                    property var value_: value ? value : null
                                    onValue_Changed: {
                                        currentIndex = indexOfValue(value_)
                                    }
                                    Component.onCompleted: currentIndex = indexOfValue(value_)
                                    onCurrentValueChanged: {
                                        if (value != currentValue) {
                                            value = currentValue;
                                        }
                                    }
                                }
                            }
                        }

                    }

                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        XsSimpleButton {
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            onClicked: {
                dialog.close()
            }
        }
    }

}
