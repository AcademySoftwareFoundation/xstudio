

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

import xstudio.qml.models 1.0
import xStudio 1.0


// This is an adapted version of XsAttributesPanel, allowing us to add a dynamic
// list of masks that the user can enable/disable

XsWindow {

	id: dialog
	width: 500
	height: thelayout.childrenRect.height+30
    title: "Field Chart Settings"
    property real cwidth: 50
    property real cwidth2: 50
    XsModuleData {
        id: field_charts_data
        modelDataName: "fieldchart_image_set"
    }

    XsAttributeValue {
        id: __all_charts
        attributeTitle: "All Charts"
        model: field_charts_data
    }
    property alias all_charts: __all_charts.value
    property var field_chart_names: Object.keys(all_charts)

    XsAttributeValue {
        id: __user_charts
        attributeTitle: "User Charts"
        model: field_charts_data
    }
    property alias user_charts: __user_charts.value
    property var user_char_names: Object.keys(user_charts)

    XsAttributeValue {
        id: __visible_charts
        attributeTitle: "Visibile Charts"
        model: field_charts_data
    }
    property alias visible_charts: __visible_charts.value


    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    function setCWidth2(ww) {
        if (ww > cwidth2) cwidth2 = ww
    }

    // make an alias so the mask options are accessible as an array
    property alias attribute_set: attribute_set

    property var deleteName
    function deleteFieldChart() {
        if (user_char_names.includes(deleteName)) {
            var v = user_charts
            delete v[deleteName]
            user_charts = v
            deleteName = undefined
        }
    }

    property var new_svg
    function doAddFieldChart_pt1(path) {

        if (path == false) return; // load was cancelled
        new_svg = path
        dialogHelpers.textInputDialog(
            doAddFieldChart_pt2,
            "New Field Chart",
            "Enter a name for the new field chart.",
            "Field Chart",
            ["Cancel", "Add"])

    }

    function doAddFieldChart_pt2(new_name, button) {

        if (button == "Add" && new_svg !== undefined) {

            if (field_chart_names.includes(new_name)) {

                dialogHelpers.errorDialogFunc("Error", "There is already a chart named \"" + new_name + "\"");
                return
            }

            var v = user_charts
            v[new_name] = new_svg,
            user_charts = v
            new_svg = undefined
        }
    }

    XsModuleData {
        id: attribute_set
        modelDataName: "fieldchart_settings"
    }

    ColumnLayout {

        id: thelayout
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
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
                text: "Field Chart Images"
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: XsStyleSheet.menuBorderColor
            }
        }

        Repeater {

            // N.B the 'combo_box_options' attribute is propagated via the
            // XsToolBox which instantiates the BasicViewportMaskButton
            model: field_chart_names
            id: r1

            RowLayout {

                id: settings_items
                Layout.alignment: Qt.AlignCenter
                Layout.margins: 0
                Layout.preferredHeight: 20

                XsCheckBox {
                    width: 20
                    height: 20
                    checked: visible_charts.includes(tt.text)
                    onClicked: {
                        var v = visible_charts
                        if (visible_charts.includes(tt.text)) {
                            let idx = v.indexOf(tt.text);
                            if (idx > -1) {
                                v.splice(idx, 1);
                            }
                        } else {
                            v.push(tt.text)
                        }
                        visible_charts = v;
                    }
                }

                XsText {
                    id: tt
                    Layout.preferredWidth: cwidth2
                    text: field_chart_names[index]
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    y: index*20
                    TextMetrics {
                        font:   tt.font
                        text:   tt.text
                        onWidthChanged: setCWidth2(width)
                    }
                }

                XsPrimaryButton {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 40
                    imgSrc: "qrc:/icons/delete.svg"
                    enabled: user_char_names.includes(tt.text)
                    onClicked: {
                        deleteName = tt.text
                        dialogHelpers.multiChoiceDialog(
                            deleteFieldChart,
                            "Remove Field Chart",
                            "Remove the Field Chart \"" + tt.text + "\"?",
                            ["Cancel", "Remove"],
                            undefined)
                    }
                }

            }

        }

        XsPrimaryButton {
            id: addBut
            Layout.preferredHeight: 30
            Layout.preferredWidth: 60
            Layout.alignment: Qt.AlignRight
            text: "Add ..."
            onClicked: {
                dialogHelpers.showFileDialog(
                    doAddFieldChart_pt1,
                    "",
                    "Select SVG File for Field Chart",
                    "svg",
                    ["SVG (*.svg)"],
                    true,
                    false)
            }
            XsToolTip {
                visible: addBut.isHovered
                text: "You can select any SVG file you wish from the file system to use as a field chart overlay."
            }
        }

        Item {
            Layout.preferredHeight: 20
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
                text: "Field Chart Settings"
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
                    model: attribute_set

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
            Layout.preferredHeight: 20
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
