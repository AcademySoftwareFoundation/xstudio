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

    property var compare_options: ["Off", "A/B", "Grid", "Wipe", "Horizontal", "Vertical", "String", "PiP"]
    property var align_options: ["Off", "On", "On (Trim)"]

    property var value__: valueRole

    // use this to prevent circular update
    property bool updating: false

    onValue__Changed: {
        if (!updating) {
            updating = true
            var idx = compare_options.indexOf(valueRole[0])
            combo_box.currentIndex = idx
            idx = align_options.indexOf(valueRole[1])
            combo_box2.currentIndex = idx
            updating = false
        }
    }

    function updateBackend() {
        if (!updating) {
            updating = true
            var v = []
            v.push(compare_options[combo_box.currentIndex])
            v.push(align_options[combo_box2.currentIndex])
            valueRole = v
            updating = false
        }
    }

    XsLabel {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
        Layout.preferredWidth: parent.width/3 //prefsLabelWidth
        Layout.maximumWidth: prefsLabelWidth
        wrapMode: Text.NoWrap
        elide: Text.ElideLeft

        text: displayNameRole
        horizontalAlignment: Text.AlignRight
    }

    RowLayout {

        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        Layout.preferredWidth: prefsLabelWidth
        Layout.minimumWidth: prefsLabelWidth/2
        Layout.fillHeight: true

        XsComboBox {

            id: combo_box
            Layout.fillHeight: true
            Layout.fillWidth: true
            model: compare_options
            onCurrentIndexChanged: {
                updateBackend()
            }
        }

        XsLabel {
            Layout.leftMargin: 10
            Layout.alignment: Qt.AlignVCenter|Qt.AlignRight    
            text: "Auto Align:"
        }

        XsComboBox {

            id: combo_box2
            Layout.fillHeight: true
            Layout.fillWidth: true
            model: align_options
            onCurrentIndexChanged: {
                updateBackend()
            }

        }
    }

    XsPreferenceInfoButton {
    }

}
