// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts




import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import Grading 2.0

ColumnLayout { id: wheelDiv
    spacing: 1

    property bool isTaller: false
    onHeightChanged: {
        if(height > 195) isTaller = true
        else isTaller = false
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
        visible: isTaller
    }

    Repeater {
        model: 3

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: visible? XsStyleSheet.widgetStdHeight : 0
            visible: isTaller

            GTValueEditor{
                width: 40+10
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
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