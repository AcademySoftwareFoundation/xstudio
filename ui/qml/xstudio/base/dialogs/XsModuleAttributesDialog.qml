// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xStudio 1.1
import xstudio.qml.module 1.0
import xstudio.qml.models 1.0

XsWindow {

	id: dialog
	width: 300
	height: r1.count*20 + 100
    property var attributesGroupNames

    centerOnOpen: true

    XsModuleAttributesModel {
        id: attribute_set
        attributesGroupNames: dialog.attributesGroupNames
    }

    /*XsModuleData {
        id: attribute_set
        modelDataName: dialog.attributesGroupNames
    }*/

    RowLayout {

        anchors.fill: parent
        anchors.margins: 10

        ColumnLayout {

            Layout.margins: 5

            Repeater {

                // N.B the 'combo_box_options' attribute is propagated via the
                // XsToolBox which instantiates the BasicViewportMaskButton
                model: attribute_set
                y: 5
                id: r1

                Rectangle {

                    height: 20
                    color: "transparent"
                    width: label_metrics.width
                    Layout.alignment: Qt.AlignRight

                    Text {
                        id: thetext
                        anchors.fill: parent
                        text: title ? title : ""
                        color: XsStyle.controlColor
                        font.family: XsStyle.controlTitleFontFamily
                        font.pixelSize: XsStyle.popupControlFontSize
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
                        XsFloatAttrSlider {
                            height: 20
                            Layout.fillWidth: true
                        }
                    }

                    DelegateChoice {
                        roleValue: "IntegerValue";
                        XsIntAttrSlider {
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
                            XsBoolAttrCheckBox {
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
                                    value = currentValue;
                                }
                            }
                        }
                    }

                }

            }
        }
    }

    RoundButton {
        id: btnOK
        text: qsTr("Close")
        width: 60
        height: 24
        radius: 5
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 5
        DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        background: Rectangle {
            radius: 5
//                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
            color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
            gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
            anchors.fill: parent
        }
        contentItem: Text {
            text: btnOK.text
            color: XsStyle.hoverColor//:XsStyle.mainColor
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            id: mouseArea
            hoverEnabled: true
            anchors.fill: btnOK
            cursorShape: Qt.PointingHandCursor
            onClicked: dialog.close()
        }
    }

}