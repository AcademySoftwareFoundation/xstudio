// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts

import xStudio 1.0


Item{ id: widget

    property alias text: labelDiv.text
    // property alias value: valueDiv.model[currentIndex]
    property alias valueDiv: valueDiv

    property alias model: valueDiv.model
    property alias currentIndex: valueDiv.currentIndex

    signal accepted()
    signal activated(index: int)

    ColumnLayout{
        anchors.fill: parent
        spacing: 5

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height/3

            XsText{ id: labelDiv
                anchors.fill: parent
                horizontalAlignment: Text.AlignLeft
                text: ""
            }
        }
        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height/1.5

            RowLayout{
                anchors.fill: parent
                spacing: 1

                Item{
                    Layout.preferredWidth: 20
                    Layout.fillHeight: true
                }
                XsComboBox{ id: valueDiv
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onCurrentIndexChanged: {
                        widget.currentIndexChanged()
                    }
                    onActivated: {
                        widget.activated(index)
                    }

                    onAccepted: widget.accepted()
                }
            }
        }
    }
}