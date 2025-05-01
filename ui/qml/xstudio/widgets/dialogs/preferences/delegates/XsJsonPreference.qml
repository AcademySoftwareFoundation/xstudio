// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

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

    XsSimpleButton {

        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        Layout.preferredWidth: 140
        Layout.minimumWidth: prefsLabelWidth/2
        Layout.fillHeight: true
        text: qsTr("Edit ...")
        width: XsStyleSheet.primaryButtonStdWidth*2
        onClicked: {
            loader.sourceComponent = dialog
            loader.item.text = JSON.stringify(valueRole)
            loader.item.visible = true
        }
    }

    Loader {
        id: loader
    }

    Component {
        id: dialog
        XsTextEditDialog {
            onAccepted: {
                valueRole = JSON.parse(text)
            }
        }
    }

    XsPreferenceInfoButton {
    }

}