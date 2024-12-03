// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

RowLayout{
    id: control
    property alias titleText: titleDiv.text
    property alias valueText: valueDiv.text
    property alias toolTipMArea: toolTipMArea
    property alias valueDiv: valueDiv
    property alias textColor: valueDiv.color
    property string toolTip: valueDiv.text

    spacing: 0

    XsText{ id: titleDiv
        text: "Title :"
        Layout.preferredWidth: parent.width/4
        Layout.maximumHeight: parent.height
        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
        horizontalAlignment: Text.AlignLeft
        color: XsStyleSheet.hintColor
        elide: Text.ElideRight
    }

    XsText{ id: valueDiv
        text: "Value"
        Layout.fillWidth: true
        Layout.preferredWidth: parent.width/1.5
        Layout.maximumHeight: parent.height
        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
        horizontalAlignment: Text.AlignRight
        color: XsStyleSheet.hintColor
        elide: Text.ElideRight

        MouseArea { id: toolTipMArea
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true
            acceptedButtons: Qt.NoButton

            XsToolTip{
                text: control.toolTip
                visible: parent.containsMouse && parent.parent.truncated
                width: parent.parent.textWidth == 0? 0 : 150
                x: 0
            }
        }
    }
}
