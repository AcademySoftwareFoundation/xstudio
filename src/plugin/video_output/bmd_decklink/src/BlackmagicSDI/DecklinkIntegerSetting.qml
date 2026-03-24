// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

ColumnLayout {

    property var integer_attr_name
    property var display_name

    XsAttributeValue {
        id: the_attr
        attributeTitle: integer_attr_name
        model: decklink_settings
    }
    property alias attr_value: the_attr.value

    XsLabel {
        text: display_name
        Layout.alignment: Qt.AlignLeft
    }

    XsTextField {

        Layout.alignment: Qt.AlignLeft
        text: attr_value
        wrapMode: Text.Wrap
        Layout.preferredWidth: 80
        clip: true
        bgColor: palette.base
        onActiveFocusChanged:{
            if(activeFocus) selectAll()
        }
        onTextChanged: {
            var v = parseInt(text)
            if (!isNaN(v) && v != attr_value) {
                attr_value = v
            }
        }

        // binding to backend
        property var backendValue: attr_value
        onBackendValueChanged: {
            var v = "" + backendValue
            if (v != text) {
                text = v
            }
        }

        onEditingFinished: {
            var v = parseInt(text)
            if (!isNaN(v) && v != attr_value) {
                attr_value = v
            }
            text = "" + attr_value
        }

    }

}