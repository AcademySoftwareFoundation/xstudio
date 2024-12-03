// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0


Item{ id: widget

    property alias text: labelDiv.text
    // property alias value: valueDiv.model[currentIndex]
    property alias valueDiv: valueDiv

    property alias model: valueDiv.model
    property alias currentIndex: valueDiv.currentIndex

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
        XsComboBox{ id: valueDiv
            Layout.fillWidth: true
            Layout.fillHeight: true

            onCurrentIndexChanged: {
                widget.currentIndexChanged()
            }
        }

    }

}