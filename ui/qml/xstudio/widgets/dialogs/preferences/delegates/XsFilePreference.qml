// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts



import xstudio.qml.models 1.0
import xStudio 1.0

import "../widgets"

RowLayout {
    
    width: parent.width
    height: XsStyleSheet.widgetStdHeight
    property var readonly: datatypeRole == "read only file path"

    XsLabel {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
        Layout.preferredWidth: prefsLabelWidth //prefsLabelWidth
        Layout.maximumWidth: prefsLabelWidth
        wrapMode: Text.NoWrap
        elide: Text.ElideLeft
        text: displayNameRole
        horizontalAlignment: Text.AlignRight
    }

    XsTextField {

        id: textField
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        text: valueRole
        wrapMode: Text.NoWrap
        Layout.preferredWidth: prefsLabelWidth*1.5
        Layout.minimumWidth: prefsLabelWidth*1.5
        Layout.fillHeight: true
        clip: true
        bgColor: palette.base
        enabled: !readonly
        onTextChanged: {
            if (text != valueRole) {
                valueRole = text
            }
        }

        // binding to backend
        property var backendValue: valueRole
        onBackendValueChanged: {
            if (!activeFocus) {
                text = backendValue
            }
        }

        onEditingFinished: {
            valueRole = text
        }

    }


    function setFilePath(path) {

        if (path == false) return; // load was cancelled
        valueRole = path
    }

    XsPrimaryButton {
        Layout.fillHeight: true
        text: "Browse ..."
        enabled: !readonly
        onClicked: {
            dialogHelpers.showFileDialog(
                setFilePath,
                "",
                "Select a File",
                "config",
                optionsRole != undefined ? optionsRole : ["All files (*)"],
                true,
                false)
        }
        toolTip: "Browse filesystem."
    }

    XsPreferenceInfoButton {
    }

}
