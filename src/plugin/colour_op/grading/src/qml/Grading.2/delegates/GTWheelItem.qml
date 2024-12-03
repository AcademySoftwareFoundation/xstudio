// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import Grading 2.0

Item { id: wheelItem
    property real dividerWidth: 2

    Rectangle{ id: bg
        anchors.fill: parent
        color: XsStyleSheet.baseColor
    }
    Rectangle{ id: divider
        width: dividerWidth
        height: parent.height
        color: XsStyleSheet.panelBgColor
        anchors.left: parent.left
    }

    ColumnLayout { id: col
        anchors.fill: bg
        anchors.margins: 1
        spacing: 1

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            Layout.maximumHeight: 2
        }
        RowLayout{ id: titleDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.secondaryButtonStdWidth + 2
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth + 2
            spacing: 1

            Item{
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            XsText { id: textDiv
                Layout.preferredWidth: textWidth
                Layout.fillHeight: true
                text: abbr_title
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
                Layout.preferredWidth: XsStyleSheet.secondaryButtonStdWidth
                Layout.fillHeight: true
                imgSrc: "qrc:/icons/rotate-ccw.svg"
                imageDiv.sourceSize.width: 14
                imageDiv.sourceSize.height: 14
                forcedHover: textMA.containsMouse

                onClicked: {
                    // 'value' and 'default_value' exposed from the model used
                    // to instantiate the wheel
                    var _value = value
                    _value[0] = default_value[0]
                    _value[1] = default_value[1]
                    _value[2] = default_value[2]
                    _value[3] = default_value[3]
                    value = _value
                }
            }
            Item{
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        RowLayout{ id: controlsDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 1

            property real sideMargin: [ (wheelItem.width - (2*2)) / 3 + 2 ] /8

            Item{
                Layout.fillWidth: true
                Layout.minimumWidth: 1
                Layout.maximumWidth: controlsDiv.sideMargin
                Layout.fillHeight: true
            }

            Loader{ id: wheelDiv
                property int defaultWheelSize: width

                Layout.fillWidth: true
                Layout.fillHeight: true

                sourceComponent: GTWheelDivHorz{ anchors.fill: wheelDiv }
            }

            Item{
                Layout.fillWidth: true
                Layout.minimumWidth: 1
                Layout.preferredWidth: 2
                Layout.maximumWidth: 4
                Layout.fillHeight: true
            }

            ColumnLayout { id: sliderDiv
                Layout.minimumWidth: 45
                Layout.preferredWidth: 45
                Layout.maximumWidth: 45
                Layout.preferredHeight: parent.height
                spacing: 1

                Item{
                    Layout.fillWidth: true
                    Layout.minimumHeight: 2
                }

                Item{
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    GTSlider{
                        id: slider
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        backend_value: value[3]
                        default_backend_value: default_value[3]
                        from: float_scrub_min[3]
                        to: float_scrub_max[3]
                        step: float_scrub_step[3]

                        onSetValue: {
                            var _value = value
                            _value[3] = newVal
                            value = _value
                        }
                    }
                }

                Item{
                    Layout.fillWidth: true
                    Layout.minimumHeight: XsStyleSheet.widgetStdHeight

                    GTValueEditor{
                        width: parent.width
                        height: parent.height
                        valueText: value[3].toFixed(3)
                        indicatorColor: "transparent"

                        onEdited:{
                            var _value = value
                            _value[3] = parseFloat(currentText)
                            value = _value
                        }
                    }
                }

            }

            Item{
                Layout.fillWidth: true
                Layout.minimumWidth: 1
                Layout.maximumWidth: controlsDiv.sideMargin
                Layout.fillHeight: true
            }

        }

    }


}