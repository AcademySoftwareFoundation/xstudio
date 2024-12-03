// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0
import xstudio.qml.models 1.0
import Grading 2.0

Item{

    property string titleText: ""

    ColumnLayout{ id: col
        anchors.fill: parent
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 2
        spacing: 1

        RowLayout{ id: titleSlider
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.secondaryButtonStdWidth + 2
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth + 2
            spacing: 0

            Item{
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            XsText { id: textDiv
                Layout.preferredWidth: textWidth
                Layout.fillHeight: true
                text: titleText
                elide: Text.ElideRight
                font.pixelSize: XsStyleSheet.fontSize*1.2
                font.bold: true

                MouseArea{ id: textMA
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked:{
                        resetButton.clicked()
                    }
                    onPressed: resetButton.down = true
                    onReleased: resetButton.down = undefined
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: 4
                Layout.fillHeight: true
            }
            XsSecondaryButton {  id: resetButton
                Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
                Layout.preferredWidth: XsStyleSheet.secondaryButtonStdWidth
                Layout.fillHeight: true
                imgSrc: "qrc:/icons/rotate-ccw.svg"
                imageDiv.sourceSize.width: 14
                imageDiv.sourceSize.height: 14
                forcedHover: textMA.containsMouse

                onClicked: {
                    value = default_value
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.minimumHeight: 2
        }

        Item{ id: controlDiv
            Layout.fillWidth: true
            Layout.fillHeight: true

            GTSlider{ id: slider
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter

                backend_value: value
                onSetValue: {
                    var _value = (typeof value == "number") ? value : value[index]
                    _value = newVal
                    if(typeof value == "number") value = _value
                    else value[index] = _value
                }
            }
        }

        Item{ id: valuesDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            GTValueEditor{
                width: 45
                height: XsStyleSheet.widgetStdHeight
                valueText: value.toFixed(3)
                indicatorColor: "transparent"
                anchors.horizontalCenter: parent.horizontalCenter

                onEdited:{
                    value = parseFloat(currentText)
                }
            }
        }

    }

}