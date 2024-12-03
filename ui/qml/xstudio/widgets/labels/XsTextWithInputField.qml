// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0


Item{ id: widget

    property alias label: labelDiv.text
    property alias value: valueDiv.text
    // property alias hintingPreference: valueDiv.font.hintingPreference
    property alias echoMode: valueDiv.echoMode

    signal editingCompleted()

    RowLayout{
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 10

        XsText{ id: labelDiv
            Layout.preferredWidth: 140
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            text: ""
            horizontalAlignment: Text.AlignRight
        }
        XsTextField{ id: valueDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: ""
            bgColor: palette.base

            onEditingFinished: editingCompleted()
        }

    }

}