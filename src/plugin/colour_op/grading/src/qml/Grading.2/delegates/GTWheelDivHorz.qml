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

ColumnLayout { id: wheelDiv
    spacing: 1

    property bool isWider: false
    onWidthChanged: {
        if(width > 150) isWider = true
        else isWider = false
    }

    Item{
        Layout.fillWidth: true
        Layout.minimumHeight: defaultWheelSize/2
        Layout.preferredHeight: defaultWheelSize
        Layout.fillHeight: true

        onWidthChanged:{
            wheel.scale = Math.min(width, height) < defaultWheelSize ?
                Math.min(width, height)/defaultWheelSize : 1
        }
        onHeightChanged:{
            wheel.scale = Math.min(width, height) < defaultWheelSize ?
                Math.min(width, height)/defaultWheelSize : 1
        }

        GTWheel {
            id: wheel
            anchors.centerIn: parent

            backend_color: value
            wheelSize: defaultWheelSize

        }
    }

    Item{
        Layout.fillWidth: true
        Layout.minimumHeight: visible? 4:0
        Layout.fillHeight: true
        visible: isWider && wheelDiv.height > 150
    }

    Item{
        Layout.fillWidth: true
        Layout.preferredHeight: visible? (XsStyleSheet.widgetStdHeight) : 0
        visible: isWider && wheelDiv.width > 150

        GridLayout{ id: rgbGrid
            rows: 1
            rowSpacing: 0
            columns: 3
            columnSpacing: 1.5
            height: XsStyleSheet.widgetStdHeight
            anchors.horizontalCenter: parent.horizontalCenter

            Repeater {
                model: 3

                GTValueEditor{
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    valueText: value[index].toFixed(4)
                    indicatorColor: index==0?"red":index==1?"green":"blue"

                    onEdited:{
                        var _value = value
                        _value[index] = parseFloat(currentText)
                        value = _value
                    }
                }
            }

        }

    }
    Item{
        Layout.fillWidth: true
        Layout.maximumHeight: 4
    }


}