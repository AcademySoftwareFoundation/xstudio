// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
    property alias text: text_control.text
    property int rejectIndex: -1
    property int acceptIndex: buttonModel ? buttonModel.length-1 : -1
    property var buttonModel: ['Cancel', 'Maybe', 'Okay']

    minimumHeight: 85
    minimumWidth: 300

    keepCentered: true
    centerOnOpen: true

    // height: (footer ? footer.height : 0) + layout.implicitHeight + (header? header.height : 0)

    signal selected(int button_index)

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        XsLabel {
            Layout.fillWidth: true
            Layout.fillHeight: visible ? true : false
            Layout.minimumHeight: 20
            Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter

        	id: text_control
            font.pixelSize: XsStyle.popupControlFontSize * 1.3

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
           	text: ""
            visible: text ? true : false
        }

        RowLayout {
            Layout.minimumHeight: 25
            Layout.minimumWidth: 100
            // Layout.maximumHeight: 25
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignVBottom|Qt.AlignHCenter

            spacing:10

            Keys.onReturnPressed: {
                selected(acceptIndex)
                accept()
            }
            Keys.onEscapePressed: {
                selected(rejectIndex)
                reject()
            }

            Repeater {
                model: buttonModel
                delegate: XsRoundButton {
                    text: modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    highlighted: index == acceptIndex
                    onClicked: {
                        selected(index)
                        if(index == rejectIndex) {
                            reject()
                        } else {
                            accept()
                        }
                    }
                }
            }
        }
    }
}