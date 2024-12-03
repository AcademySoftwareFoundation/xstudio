// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0


Item{ id: widget

    property alias text: labelDiv.text
    property alias valueDiv: valueDiv
    property alias checked: checkBoxDiv.checked

    property alias model: valueDiv.model
    property alias currentIndex: valueDiv.currentIndex

    signal activated(index: int)

    RowLayout{
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 10

        XsCheckBox { id: checkBoxDiv
            Layout.preferredWidth: height - parent.spacing
            Layout.fillHeight: true
            forcedHover: labelMArea.containsMouse
            padding: 0
            topInset: 0
            bottomInset: 0
            checked: false
            onClicked: {

            }
        }
        XsText{ id: labelDiv
            Layout.preferredWidth: textWidth
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            text: ""
            horizontalAlignment: Text.AlignLeft
            opacity: checked? 1 : 0.7

            MouseArea{ id: labelMArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    checked = !checked
                }
            }
        }
        XsComboBox{ id: valueDiv
            Layout.fillWidth: true
            Layout.preferredHeight: checkBoxDiv.height

            enabled: checked
            onCurrentIndexChanged: {
                widget.currentIndexChanged()
            }
            onActivated: {
                widget.activated(index)
            }
        }
    }

}