// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

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

    XsCheckBox {

        id: checkBox
        Layout.preferredWidth: 22
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        checked: valueRole
        onClicked: {
            valueRole = !valueRole
        }

    }

    XsPreferenceInfoButton {
    }

}
