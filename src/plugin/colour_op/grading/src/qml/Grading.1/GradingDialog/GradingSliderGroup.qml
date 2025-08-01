// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0

Item {
    id: root

    width: Math.max(titleRow.width, sliderList.width)
    height: titleRow.height + sliderList.height

    property string title
    property string attr_group
    property string attr_suffix
    property real fixed_size: -1

    XsModuleAttributesModel {
        id: attr_model
        attributesGroupNames: root.attr_group
    }
    XsModuleAttributes {
        id: attr
        attributesGroupNames: root.attr_group

        onAttrAdded: {
            // Master is added last, we know all the other attributes are here
            if (attr_name.includes("master")) {
                inputRed.text    = Qt.binding(function() { return attr["red_"    + root.attr_suffix].toFixed(5) })
                inputGreen.text  = Qt.binding(function() { return attr["green_"  + root.attr_suffix].toFixed(5) })
                inputBlue.text   = Qt.binding(function() { return attr["blue_"   + root.attr_suffix].toFixed(5) })
                inputMaster.text = Qt.binding(function() { return attr["master_" + root.attr_suffix].toFixed(5) })
            } else if (attr_name === "saturation") {
                inputRed.text   = Qt.binding(function() { return attr[attr_name].toFixed(5) })
            }
        }
    }
    XsModuleAttributes {
        id: attr_default_value
        attributesGroupNames: root.attr_group
        roleName: "default_value"
    }
    XsModuleAttributes {
        id: attr_float_scrub_min
        attributesGroupNames: root.attr_group
        roleName: "float_scrub_min"

        onAttrAdded: {
            if (attr_name.includes("master")) {
                inputRed.validator.bottom    = Qt.binding(function() { return attr_float_scrub_min["red_"    + root.attr_suffix] })
                inputGreen.validator.bottom  = Qt.binding(function() { return attr_float_scrub_min["green_"  + root.attr_suffix] })
                inputBlue.validator.bottom   = Qt.binding(function() { return attr_float_scrub_min["blue_"   + root.attr_suffix] })
                inputMaster.validator.bottom = Qt.binding(function() { return attr_float_scrub_min["master_" + root.attr_suffix] })
            }
        }
    }
    XsModuleAttributes {
        id: attr_float_scrub_max
        attributesGroupNames: root.attr_group
        roleName: "float_scrub_max"

        onAttrAdded: {
            if (attr_name.includes("master")) {
                inputRed.validator.top    = Qt.binding(function() { return attr_float_scrub_max["red_"    + root.attr_suffix] })
                inputGreen.validator.top  = Qt.binding(function() { return attr_float_scrub_max["green_"  + root.attr_suffix] })
                inputBlue.validator.top   = Qt.binding(function() { return attr_float_scrub_max["blue_"   + root.attr_suffix] })
                inputMaster.validator.top = Qt.binding(function() { return attr_float_scrub_max["master_" + root.attr_suffix] })
            }
        }
    }

    Column {
        anchors.topMargin: 5
        anchors.fill: parent

        Row {
            id: titleRow
            spacing: 10
            anchors.horizontalCenter: parent.horizontalCenter
            height: 30

            Text {
                text: root.title
                font.pixelSize: 20
                color: "white"
            }

            XsButton {
                id: reloadButton
                width: 20; height: 20
                bgColorNormal: "transparent"
                borderWidth: 0

                onClicked: {
                    if (root.title === "Sat") {
                        attr["saturation"] = attr_default_value["saturation"]
                    }
                    else {
                        attr["red_"    + root.attr_suffix] = attr_default_value["red_"    + root.attr_suffix]
                        attr["green_"  + root.attr_suffix] = attr_default_value["green_"  + root.attr_suffix]
                        attr["blue_"   + root.attr_suffix] = attr_default_value["blue_"   + root.attr_suffix]
                        attr["master_" + root.attr_suffix] = attr_default_value["master_" + root.attr_suffix]
                    }
                }

                Image {
                    source: "qrc:/feather_icons/rotate-ccw.svg"

                    layer {
                        enabled: true
                        effect: ColorOverlay {
                            color: reloadButton.down || reloadButton.hovered ? "white" : XsStyle.controlTitleColor
                        }
                    }
                }
            }
        }

        ListView {
            id: sliderList
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: -8
            width: root.fixed_size > 0 ? root.fixed_size : contentItem.childrenRect.width
            height: 155

            orientation: Qt.Horizontal
            model: attr_model
            delegate: GradingVSlider {}
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            leftPadding: 20

            Column {
                id: sliderInputCol

                XsTextField {
                    id: inputRed
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        if (root.title === "Sat") {
                            attr["saturation"] = parseFloat(text)
                        }
                        else {
                            attr["red_" + root.attr_suffix] = parseFloat(text)
                        }
                    }
                }
                XsTextField {
                    id: inputGreen
                    visible: root.title != "Sat"
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["green_" + root.attr_suffix] = parseFloat(text)
                    }
                }
                XsTextField {
                    id: inputBlue
                    visible: root.title != "Sat"
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["blue_" + root.attr_suffix] = parseFloat(text)
                    }
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 5

                Label {
                    visible: root.title != "Sat"
                    text: root.title == "Offset" ? "+" : "x"
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                XsTextField {
                    id: inputMaster
                    visible: root.title != "Sat"
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["master_" + root.attr_suffix] = parseFloat(text)
                    }
                }
            }
        }
    }
}
