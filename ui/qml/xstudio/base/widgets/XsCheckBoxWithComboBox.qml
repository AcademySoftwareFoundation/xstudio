// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15

import xStudio 1.1

Control {
    id: widget

    property alias text: checkBox.text
    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    // property alias model: editable? activeComboBox.model: comboBox.model
    // property alias activeComboBox: editable? editableComboBox: comboBox
    // property alias editable: editableComboBox.editable
    property alias model: editableComboBox.model
    property alias textRole: editableComboBox.textRole
    property alias editable: editableComboBox.editable
    property alias checked: checkBox.checked
    property alias currentIndex: editableComboBox.currentIndex

    XsCheckbox{ id: checkBox
        forcedTextHover: parent.hovered
        // width: 80
        height: parent.height
        enabled: widget.enabled
    }

    // XsComboBox{ id: comboBox
    //     visible: !editable
    //     height: parent.height
    //     anchors.left: checkBox.right
    //     anchors.right: parent.right
    //     anchors.rightMargin: 12
    // }

    XsComboBox{ id: editableComboBox
        enabled: checkBox.checked
        height: parent.height
        anchors.left: checkBox.right
        anchors.right: parent.right
    }

}



