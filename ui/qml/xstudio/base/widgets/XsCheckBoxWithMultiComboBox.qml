// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15

import xStudio 1.1

Control {
    id: widget

    property alias text: checkBox.text
    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    property alias model: multiComboBox.valuesModel
    property alias hint: multiComboBox.hint
    property alias checked: checkBox.checked
    property alias popup: multiComboBox.popup
    property alias checkedIndexes: multiComboBox.checkedIndexes

    signal hide()
    onHide:{
        multiComboBox.close()
    }

    XsCheckbox{ id: checkBox
        forcedTextHover: parent.hovered
        // width: 80
        height: parent.height
        enabled: widget.enabled
        onCheckedChanged:{
            hide()
        }
    }

    XsComboBoxMultiSelect{ id: multiComboBox
        enabled: checkBox.checked
        forcedBg: checkBox.checked
        height: parent.height
        anchors.left: checkBox.right
        anchors.right: parent.right
        hint: "multi-input"
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        // height: itemHeight
    }
}



