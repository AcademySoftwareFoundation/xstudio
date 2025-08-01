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
import xstudio.qml.clipboard 1.0

import MaskTool 1.0

XsWindow {

    id: drawDialog
    title: "Colour Correction Tools"
    // title: attr.grading_layer ? "Colour Correction Tools - " + attr.grading_layer : "Colour Correction Tools"

    width: minimumWidth
    minimumWidth: attr.mvp_1_release != undefined ? 650 : 850
    maximumWidth: minimumWidth

    height: minimumHeight
    minimumHeight: attr.mvp_1_release != undefined ? 320 : 340
    maximumHeight: minimumHeight

    onVisibleChanged: {
        if (!visible) {
            // ensure keyboard events are returned to the viewport
            sessionWidget.playerWidget.viewport.forceActiveFocus()
        }
    }

    XsModuleAttributes {
        id: attr
        attributesGroupNames: "grading_settings"
    }

    XsModuleAttributes {
        id: attr_layers
        attributesGroupNames: "grading_layers"
        roleName: "combo_box_options"
    }

    FileDialog {
        id: cdl_save_dialog
        title: "Save CDL"
        defaultSuffix: "cdl"
        folder: shortcuts.home
        nameFilters:  [ "CDL files (*.cdl)", "CC files (*.cc)", "CCC files (*.ccc)" ]
        selectExisting: false

        onAccepted: {
            // defaultSuffix doesn't seem to work in the current Qt version used
            var path = fileUrl.toString()
            if (!path.endsWith(".cdl") && !path.endsWith(".cc") && !path.endsWith(".ccc")) {
                path += ".cdl"
            }

            attr.grading_action = "Save CDL " + path
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3

        MaskDialog {
            id: maskDialog

            enabled: attr.mask_tool_active ? attr.mask_tool_active : false
            visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false

            Layout.minimumWidth: 190
            Layout.maximumWidth: 190
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.topMargin: 1
            spacing: 3

            Rectangle {
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30
                Layout.fillWidth: true

                color: "transparent"
                opacity: 1.0
                border.width: 1
                border.color: Qt.rgba(
                    XsStyle.menuBorderColor.r,
                    XsStyle.menuBorderColor.g,
                    XsStyle.menuBorderColor.b,
                    0.3)
                radius: 2

                RowLayout {
                    anchors.fill: parent
                    Layout.topMargin: 0
                    spacing: 3

                    XsButton {
                        text: "Mask"
                        textDiv.font.bold: true
                        tooltip: "Enable masking, default mask starts empty"
                        isActive: attr.mask_tool_active ? attr.mask_tool_active : false
                        visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false
                        Layout.maximumHeight: 30

                        onClicked: {
                            attr.mask_tool_active = !attr.mask_tool_active
                        }
                    }

                    XsButton {
                        text: "Basic"
                        textDiv.font.bold: true
                        tooltip: "Basic grading controls (restricted to work within a single CDL)"
                        isActive: attr.grading_panel ? attr.grading_panel == "Basic" : false
                        Layout.maximumWidth: 70
                        Layout.maximumHeight: 30

                        onClicked: {
                            attr.grading_panel = "Basic"
                        }
                    }

                    XsButton {
                        text: "Sliders"
                        textDiv.font.bold: true
                        tooltip: "CDL sliders controls"
                        isActive: attr.grading_panel ? attr.grading_panel == "Sliders" : false
                        Layout.maximumWidth: 70
                        Layout.maximumHeight: 30

                        onClicked: {
                            attr.grading_panel = "Sliders"
                        }
                    }

                    XsButton {
                        text: "Wheels"
                        textDiv.font.bold: true
                        tooltip: "CDL colour wheels controls"
                        isActive: attr.grading_panel ? attr.grading_panel == "Wheels" : false
                        Layout.maximumWidth: 70
                        Layout.maximumHeight: 30

                        onClicked: {
                            attr.grading_panel = "Wheels"
                        }
                    }

                    Item {
                        // Spacer item
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }

            Rectangle {
                Layout.leftMargin: 0
                Layout.bottomMargin: 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                color: "transparent"
                opacity: 1.0
                border.width: 1
                border.color: Qt.rgba(
                    XsStyle.menuBorderColor.r,
                    XsStyle.menuBorderColor.g,
                    XsStyle.menuBorderColor.b,
                    0.3)
                radius: 2

                visible: attr.grading_panel ? attr.grading_panel == "Basic" : false

                Column {
                    anchors.fill: parent

                    GradingSliderSimple {
                        attr_group: "grading_simple"
                    }

                }
            }

            Rectangle {
                Layout.leftMargin: 0
                Layout.bottomMargin: 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                color: "transparent"
                opacity: 1.0
                border.width: 1
                border.color: Qt.rgba(
                    XsStyle.menuBorderColor.r,
                    XsStyle.menuBorderColor.g,
                    XsStyle.menuBorderColor.b,
                    0.3)
                radius: 2

                visible: attr.grading_panel ? attr.grading_panel == "Sliders" : false

                Row {
                    anchors.topMargin: 10
                    anchors.leftMargin: 10
                    anchors.fill: parent
                    spacing: 15

                    GradingSliderGroup {
                        title: "Slope"
                        fixed_size: 160
                        attr_group: "grading_slope"
                        attr_suffix: "slope"
                    }

                    GradingSliderGroup {
                        title: "Offset"
                        fixed_size: 160
                        attr_group: "grading_offset"
                        attr_suffix: "offset"
                    }

                    GradingSliderGroup {
                        title: "Power"
                        fixed_size: 160
                        attr_group: "grading_power"
                        attr_suffix: "power"
                    }

                    GradingSliderGroup {
                        title: "Sat"
                        fixed_size: 60
                        attr_group: "grading_saturation"
                    }
                }
            }

            Rectangle {
                Layout.leftMargin: 0
                Layout.bottomMargin: 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                color: "transparent"
                opacity: 1.0
                border.width: 1
                border.color: Qt.rgba(
                    XsStyle.menuBorderColor.r,
                    XsStyle.menuBorderColor.g,
                    XsStyle.menuBorderColor.b,
                    0.3)
                radius: 2

                visible: attr.grading_panel ? attr.grading_panel == "Wheels" : false

                Row {
                    anchors.topMargin: 10
                    anchors.leftMargin: 10
                    anchors.fill: parent
                    spacing: 15

                    GradingWheel {
                        title : "Slope"
                        attr_group: "grading_slope"
                        attr_suffix: "slope"
                    }

                    GradingWheel {
                        title: "Offset"
                        attr_group: "grading_offset"
                        attr_suffix: "offset"
                    }

                    GradingWheel {
                        title: "Power"
                        attr_group: "grading_power"
                        attr_suffix: "power"
                    }

                    GradingSliderGroup {
                        title: "Sat"
                        fixed_size: 60
                        attr_group: "grading_saturation"
                    }
                }
            }

            Rectangle {
                color: "transparent"
                opacity: 1.0
                border.width: 1
                border.color: Qt.rgba(
                    XsStyle.menuBorderColor.r,
                    XsStyle.menuBorderColor.g,
                    XsStyle.menuBorderColor.b,
                    0.3)
                radius: 2

                Layout.topMargin: 1
                Layout.minimumHeight: 25
                Layout.maximumHeight: 25
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    layoutDirection: Qt.RightToLeft

                    XsButton {
                        Layout.maximumWidth: 50
                        Layout.maximumHeight: 25
                        text: "Bypass"
                        tooltip: "Apply CDL or not"
                        isActive: attr.drawing_bypass ? attr.drawing_bypass : false

                        onClicked: {
                            attr.drawing_bypass = !attr.drawing_bypass
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 58
                        Layout.maximumHeight: 25
                        text: "Reset All"
                        tooltip: "Reset CDL parameters to default"

                        onClicked: {
                            attr.grading_action = "Clear"
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: "Copy"
                        tooltip: "Copy current colour correction"

                        onClicked: {
                            var grade_str = ""
                            grade_str += attr.red_slope     + " "
                            grade_str += attr.green_slope   + " "
                            grade_str += attr.blue_slope    + " "
                            grade_str += attr.master_slope  + " "
                            grade_str += attr.red_offset    + " "
                            grade_str += attr.green_offset  + " "
                            grade_str += attr.blue_offset   + " "
                            grade_str += attr.master_offset + " "
                            grade_str += attr.red_power     + " "
                            grade_str += attr.green_power   + " "
                            grade_str += attr.blue_power    + " "
                            grade_str += attr.master_power  + " "
                            grade_str += attr.saturation
                            attr.grading_buffer = grade_str
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: "Paste"
                        tooltip: "Paste colour correction"
                        enabled: attr.grading_buffer ? attr.grading_buffer != "" : false

                        onClicked: {
                            if (attr.grading_buffer) {
                                var cdl_items = attr.grading_buffer.split(" ")
                                if (cdl_items.length == 13) {
                                    attr.red_slope     = parseFloat(cdl_items[0])
                                    attr.green_slope   = parseFloat(cdl_items[1])
                                    attr.blue_slope    = parseFloat(cdl_items[2])
                                    attr.master_slope  = parseFloat(cdl_items[3])
                                    attr.red_offset    = parseFloat(cdl_items[4])
                                    attr.green_offset  = parseFloat(cdl_items[5])
                                    attr.blue_offset   = parseFloat(cdl_items[6])
                                    attr.master_offset = parseFloat(cdl_items[7])
                                    attr.red_power     = parseFloat(cdl_items[8])
                                    attr.green_power   = parseFloat(cdl_items[9])
                                    attr.blue_power    = parseFloat(cdl_items[10])
                                    attr.master_power  = parseFloat(cdl_items[11])
                                    attr.saturation    = parseFloat(cdl_items[12])
                                }
                            }
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: ">"
                        tooltip: "Toggle to next layer"
                        enabled: attr.grading_layer && attr_layers.grading_layer ? parseInt(attr.grading_layer.slice(-1)) < attr_layers.grading_layer.length : false
                        visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false

                        onClicked: {
                            attr.grading_action = "Next Layer"
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: "<"
                        tooltip: "Toggle to prev layer"
                        enabled: attr.grading_layer ? parseInt(attr.grading_layer.slice(-1)) >= 2 : false
                        visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false

                        onClicked: {
                            attr.grading_action = "Prev Layer"
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: "+"
                        tooltip: "Add a grade layer on top"
                        enabled: attr_layers.grading_layer ? attr_layers.grading_layer.length < 8 : false
                        visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false

                        onClicked: {
                            attr.grading_action = "Add Layer"
                        }
                    }

                    XsButton {
                        Layout.maximumWidth: 40
                        Layout.maximumHeight: 25
                        text: "-"
                        tooltip: "Remove the top grade layer"
                        enabled: attr_layers.grading_layer ? attr_layers.grading_layer.length > 1 : false
                        visible: attr.mvp_1_release != undefined ? !attr.mvp_1_release : false

                        onClicked: {
                            attr.grading_action = "Remove Layer"
                        }
                    }

                    Item {
                        // Spacer item
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    XsButton {
                        Layout.maximumWidth: 80
                        Layout.maximumHeight: 25
                        text: "Save CDL ..."
                        tooltip: "Save CDL to disk as a .cdl, .cc or .ccc"

                        onClicked: {
                            cdl_save_dialog.open()
                        }
                    }

                    XsButton {
                        Layout.minimumWidth: 110
                        Layout.maximumWidth: 120
                        Layout.maximumHeight: 25
                        text: "Copy Nuke Node"
                        tooltip: "Copy CDL as a Nuke OCIOCDLTransform node to the clipboard - paste into Nuke node graph with CTRL+V"

                        onClicked: {
                            var cdl_node = "OCIOCDLTransform {\n"
                            cdl_node += "  slope { "
                            cdl_node += (attr.red_slope   * attr.master_slope) + " "
                            cdl_node += (attr.green_slope * attr.master_slope) + " "
                            cdl_node += (attr.blue_slope  * attr.master_slope) + " "
                            cdl_node += "}\n"
                            cdl_node += "  offset { "
                            cdl_node += (attr.red_offset   + attr.master_offset) + " "
                            cdl_node += (attr.green_offset + attr.master_offset) + " "
                            cdl_node += (attr.blue_offset  + attr.master_offset) + " "
                            cdl_node += "}\n"
                            cdl_node += "  power { "
                            cdl_node += (attr.red_power   * attr.master_power) + " "
                            cdl_node += (attr.green_power * attr.master_power) + " "
                            cdl_node += (attr.blue_power  * attr.master_power) + " "
                            cdl_node += "}\n"
                            cdl_node += "  saturation " + attr.saturation + "\n"
                            cdl_node += "}"

                            clipboard.text = cdl_node
                        }
                    }

                }
            }
        }

    }
}