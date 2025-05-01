// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import Qt.labs.qmlmodels

import xStudio 1.0
import xstudio.qml.models 1.0

    
Item {

    id: top_level_item
    height: the_layout.childrenRect.height


    property real cwidth: 50

    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    property var rowHeight: 20

    property var editedField
    function setFieldLabel(new_name, button) {
        if (button == "Set") {
            actionAttr = ["CHANGE FIELD LABEL", editedField, new_name]
        }
    }
    function deleteField(button) {
        if (button == "Remove") {
            actionAttr = ["TOGGLE METADATA FIELD SELECTION", editedField]
        }
    }

    property var screenPositions: ["BottomLeft", "BottomCenter", "BottomRight", "TopLeft", "TopCenter", "TopRight", "FullScreen"]


    property var num_items: profile_data.length

    ColumnLayout {
        id: the_layout
        width: parent.width

        Repeater {
            model: num_items
            RowLayout {

                id: sub_layout
                property var vname: profile_data[index]["metadata_field_label"]
                property var screen_position: profile_data[index]["screen_position"]
                property var value: profile_field_values[index]
                Layout.fillWidth: true

                Item {

                    Layout.preferredWidth: cwidth + 200
                    Layout.preferredHeight: rowHeight
                    property var my_index: index
                    id: parentItem

                    XsText {
                        height: rowHeight
                        id: pname
                        text: vname
                        font.family: XsStyleSheet.altFixedWidthFontFamily
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter    
                        TextMetrics {
                            font:   pname.font
                            text:   pname.text
                            onWidthChanged: setCWidth(width + 20)
                        }
                    }

                    XsPrimaryButton {

                        //opacity: ma2.containsMouse
                        id: editButton
                        anchors.right: deleteButton.left
                        anchors.margins: 8
                        width: 20
                        height: 20
                        imgSrc: "qrc:/icons/edit.svg"
                        onClicked: {
                            editedField = profile_data[index]["metadata_field"]
                            dialogHelpers.textInputDialog(
                                setFieldLabel,
                                "Set Metadata Field Label",
                                "Provide a new label for this metadata field.",
                                vname,
                                ["Cancel", "Set"])
                        }
                    }

                    XsPrimaryButton {

                        id: deleteButton
                        //opacity: ma2.containsMouse
                        anchors.right: screenPos.left
                        anchors.margins: 8
                        width: 20
                        height: 20
                        imgSrc: "qrc:/icons/delete.svg"
                        onClicked: {
                            editedField = profile_data[index]["metadata_field"]
                            dialogHelpers.multiChoiceDialog(
                                deleteField,
                                "Remove Metata Field",
                                "Do you want to remove the field " + (vname == "" ? "(unlabelled)" : vname) + " with current value \"" + value + "\" ?",
                                ["Cancel", "Remove"],
                                undefined)
                        }
                    }

                    XsComboBox {

                        id: screenPos            
                        model: screenPositions
                        width: 100
                        height: 20
                        anchors.right: parent.right
                        currentIndex: screen_position

                        onActivated: {
                             editedField = profile_data[parentItem.my_index]["metadata_field"]
                             actionAttr = ["CHANGE SCREEN POSITION", editedField, screenPositions[index]]
                        }
                
                    }

                    /*MouseArea {
                        id: ma2
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                    }*/



                }

                XsText {

                    id: valueText
                    Layout.preferredHeight: rowHeight
                    Layout.fillWidth: true
                    text: value ? value.replace(/(\r\n|\n|\r)/gm, "") : ""
                    font.family: XsStyleSheet.altFixedWidthFontFamily
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter    
                    MouseArea {
                        id: ma
                        anchors.fill: parent
                        hoverEnabled: true
                        onContainsMouseChanged: {
                            if (containsMouse) {
                                if (valueText.truncated) {
                                    var pt = mapToItem(toolTip.parent, mouseX, mouseY);
                                    toolTip.x = pt.x
                                    toolTip.y = pt.y
                                    toolTip.text = value
                                    toolTip.show()
                                } else {
                                    toolTip.hide()
                                }
                            } else if (valueText.truncated && toolTip.text == value) {
                                toolTip.hide()
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: valueText.truncated && ma.containsMouse
                        color: "transparent"
                        border.color: palette.highlight
                    }
                }        
            }
        }

    }
}