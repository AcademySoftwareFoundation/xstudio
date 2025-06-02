// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts



import xstudio.qml.models 1.0
import xStudio 1.0

import "../widgets"

RowLayout {

    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    property var value__: valueRole
    onValue__Changed: {
        combo_box.backendValueChanged()
    }

    function spangle(oof) {
        if (valueRole != oof) {
            valueRole = oof
        }

    }

    XsLabel {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
        Layout.preferredWidth: prefsLabelWidth //prefsLabelWidth
        Layout.maximumWidth: prefsLabelWidth
        wrapMode: Text.NoWrap
        elide: Text.ElideLeft
        text: displayNameRole
        horizontalAlignment: Text.AlignRight
    }

    XsComboBox {

        id: combo_box
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        Layout.preferredWidth: Math.min(unElidedTextWidth+40, prefsLabelWidth)
        Layout.minimumWidth: 50
        Layout.fillHeight: true
        model: optionsRole
        property bool settingFromBackend: false

        onCurrentIndexChanged: {
            if (!settingFromBackend && optionsRole != undefined) {
                spangle(optionsRole[currentIndex])
            }
        }

        function backendValueChanged() {
            settingFromBackend = true
            if (optionsRole != undefined && optionsRole.indexOf(value__) != -1) {
                currentIndex = optionsRole.indexOf(value__)
            }
            settingFromBackend = false
        }

        XsToolTip{
            id: toolTip
            text: value__
            visible: combo_box.hovered && combo_box.textTruncated
            x: 0 //#TODO: flex/pointer
        }
    
    
    }

    XsPreferenceInfoButton {
    }

}
