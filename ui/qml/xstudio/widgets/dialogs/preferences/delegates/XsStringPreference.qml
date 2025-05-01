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
        Layout.preferredWidth: parent.width/2 //prefsLabelWidth
        Layout.maximumWidth: parent.width/2

        text: displayNameRole ? displayNameRole : nameRole
        horizontalAlignment: Text.AlignRight
    }

    XsTextField {

        id: textField
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        text: valueRole
        wrapMode: Text.Wrap
        Layout.preferredWidth: prefsLabelWidth //140
        Layout.minimumWidth: prefsLabelWidth/2
        Layout.fillHeight: true
        clip: true
        focus: true
        onActiveFocusChanged:{
            if(activeFocus) selectAll()
        }
        onTextChanged: {
            if (text != valueRole) {
                valueRole = text
            }
        }

        // binding to backend
        property var backendValue: valueRole
        onBackendValueChanged: {
            if (backendValue != text) {
                text = backendValue
            }
        }

        onEditingFinished: {
            valueRole = text
        }

    }

    XsPreferenceInfoButton {
    }

}
