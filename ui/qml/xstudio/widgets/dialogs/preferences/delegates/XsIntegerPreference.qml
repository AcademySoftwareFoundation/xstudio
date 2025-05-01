// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts



import xstudio.qml.models 1.0
import xStudio 1.0

import "../widgets"

RowLayout {
    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    XsLabel {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
        text: displayNameRole ? displayNameRole : nameRole
        horizontalAlignment: Text.AlignRight
        Layout.preferredWidth: parent.width/2 //prefsLabelWidth
        Layout.maximumWidth: parent.width/2
    }

    XsTextField {

        id: textField
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        text: valueRole
        wrapMode: Text.Wrap
        Layout.preferredWidth: prefsLabelWidth
        Layout.minimumWidth: prefsLabelWidth/2
        Layout.fillHeight: true
        clip: true
        bgColor: palette.base
        onActiveFocusChanged:{
            if(activeFocus) selectAll()
        }
        onTextChanged: {
            var v = parseInt(text)
            if (!isNaN(v) && v != valueRole) {
                valueRole = v
            }
        }

        // binding to backend
        property var backendValue: valueRole
        onBackendValueChanged: {
            var v = "" + backendValue
            if (v != text) {
                text = v
            }
        }

        onEditingFinished: {
            var v = parseInt(text)
            if (!isNaN(v) && v != valueRole) {
                valueRole = v
            }
            text = "" + valueRole
        }

    }

    XsPreferenceInfoButton {
    }

}
