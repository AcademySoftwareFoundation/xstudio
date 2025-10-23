// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic 
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

import xStudio 1.0
import xstudio.qml.models 1.0

    
Item {

    id: top_level_item
    property var root: metadataSet ? setKey ? metadataSet[setKey] : undefined : undefined
    property var setKey
    property var metadataSet

    property var keys: root == undefined ? [] : Object.keys(root)
    property var num_keys: keys.length
    height: the_layout.childrenRect.height

    property real cwidth: 50

    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    property var rowHeight: 20

    function addMetadataField(path) {
        if (button == "Add") {
            // setting the action attr is a way to callback to C++ side
            actionAttr = ["ADD METADATA FIELD", path]
        }
    }

    XsText {
        x: 40
        y: 40
        text: "No " + setKey << " for on-screen item."
        visible: keys.length == 0
    }

    ColumnLayout {
        id: the_layout
        width: parent.width

        Repeater {
            model: num_keys
            RowLayout {

                id: sub_layout
                property var key: keys[index]
                property var path: root[key]["path"]
                property var value: root[key]["value"]

                Layout.fillWidth: true
                XsCheckBoxSimple {
                    visible: select_mode
                    Layout.preferredHeight: rowHeight
                    Layout.preferredWidth: rowHeight
                    property var checkedOverride: profile_field_paths.includes(path)
                    onCheckedOverrideChanged: {
                        // when you click, qml sets checked and kills any 
                        // binding we try to set-up on that attr
                        checked = checkedOverride
                    }
                    onClicked: {
                        actionAttr = ["TOGGLE METADATA FIELD SELECTION", path]
                    }
                }

                Item {
                    width: 14
                }

                XsTextCopyable {
                    Layout.preferredWidth: cwidth
                    Layout.preferredHeight: rowHeight
                    id: pname
                    text: key
                    font.family: XsStyleSheet.altFixedWidthFontFamily
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter    
                    TextMetrics {
                        font:   pname.font
                        text:   pname.text
                        onWidthChanged: setCWidth(width)
                    }
                }

                XsTextCopyable {

                    id: valueText
                    Layout.preferredHeight: rowHeight
                    Layout.fillWidth: true
                    text: value.replace(/(\r\n|\n|\r)/gm, "");
                    font.family: XsStyleSheet.altFixedWidthFontFamily
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter    
                }        
            }
        }

    }
}