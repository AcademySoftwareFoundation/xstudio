// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

import xStudio 1.0
import xstudio.qml.models 1.0
import "."

// This is an adapted version of XsAttributesPanel, allowing us to add a dynamic
// list of masks that the user can enable/disable

XsWindow {

	id: dialog
	width: 500
	height: thelayout.childrenRect.height+30
    title: "Metadata HUD Settings"
    property real cwidth: 50
    property real cwidth2: 50

    minimumWidth: 540
    minimumHeight: 460

    XsModuleData {
        id: settings_attrs
        modelDataName: "Media Metadata Settings"
    }

    XsModuleData {
        id: metadata_attrs
        modelDataName: "metadata_hud_attrs"
    }

    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    XsAttributeValue {
        id: __action
        attributeTitle: "Action"
        model: metadata_attrs
    }
    property alias actionAttr: __action.value

    // make an alias so the mask options are accessible as an array
    property alias settings_attrs: settings_attrs

    Loader {
        id: loader        
    }

    Component {
        id: metadata_viewer
        MediaMetadataHUDConfig {
        }
    }

    function addNewProfile(new_name, button) {
        if (button == "Add") {
            // setting the action attr is a way to callback to C++ side
            actionAttr = ["ADD PROFILE", new_name]
        }
    }

    function deleteProfile(button) {
        if (button == "Remove") {
            // setting the action attr is a way to callback to C++ side
            actionAttr = ["DELETE CURRENT_PROFILE"]
        }
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
                text: "Metadata HUD Profile"
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

                Rectangle {

                    height: 20
                    color: "transparent"
                    width: label_metrics.width
                    onWidthChanged: setCWidth(width)
                    Layout.alignment: Qt.AlignRight

                    XsText {
                        id: thetext
                        anchors.fill: parent
                        text: "Current Profile"
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

            RowLayout {

                Layout.fillWidth: true
                Layout.margins: 5
              
                XsAttrComboBox {
                    id: current_profile
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 150
                    attr_title: "Current Profile"
                    attr_model_name: "metadata_hud_attrs"
                }

                XsPrimaryButton {

                    id: addBtn
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    imgSrc: "qrc:/icons/add.svg"
                    onClicked: {
                        dialogHelpers.textInputDialog(
                            addNewProfile,
                            "Add new HUD Metadata Profile",
                            "Please provide a name for a new Metadata HUD profile",
                            "New Profile",
                            ["Cancel", "Add"])
                    }
                    XsToolTip {
                        visible: addBtn.isHovered
                        text: "Add a new Metadata HUD profile, to show a specific set of media metadata on screen that you can configure."
                        maxWidth: dialog.width*0.75
                    }
                }

                XsPrimaryButton {

                    id: delBtn
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    imgSrc: "qrc:/icons/delete.svg"
                    enabled: current_profile.count > 1
                    onClicked: {
                        dialogHelpers.multiChoiceDialog(
                            deleteProfile,
                            "Remove Profile",
                            "Are you sure that you want to remove profile \"" + current_profile.currentText + "\"?",
                            ["Cancel", "Remove"],
                            undefined)
                    }
                    XsToolTip {
                        visible: delBtn.isHovered
                        text: "Delete the current HUD profile from your list."
                    }
                }
        
                XsPrimaryButton {
                    id: configBtn
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    imgSrc: "qrc:/icons/settings.svg"
                    onClicked: {
                        loader.sourceComponent = metadata_viewer
                        loader.item.visible = true
                    }
                    XsToolTip {
                        x: configBtn.width
                        y: configBtn.height
                        visible: configBtn.isHovered
                        text: "Configure the current profile."
                    }
                }

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
                text: "Display Settings"
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
                    model: settings_attrs

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

                    model: settings_attrs
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
