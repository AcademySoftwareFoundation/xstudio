// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

	id: dialog
	width: 450
	height: r1.count*20 + 120


    property var attributesGroupName

    XsModuleData {
        id: attribute_set
        modelDataName: attributesGroupName
    }

    property bool self_destroy: false

    onVisibleChanged: {
        if (!visible) {
            destroy()
        }
    }

    RowLayout {

        anchors.fill: parent
        anchors.margins: 10

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

    XsSimpleButton {
        text: qsTr("Close")
        width: XsStyleSheet.primaryButtonStdWidth*2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        onClicked: {
            dialog.close()
        }
    }

}