// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    id: control
    property var value_: value ? value : ""
    color: "transparent"

    function setValue(v) {
        value = v
    }


    Rectangle {

        id: text_value
        color: XsStyle.mediaInfoBarOffsetBgColour
        border.width: 1
        border.color: ma3.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
        height: valueInput.font.pixelSize*1.4
        anchors.fill: parent

        MouseArea {
            id: ma3
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                popup.toggleShow()
            }
    
        }

        Text {

            id: valueInput
            text: value_ // 'value' property provided via the xStudio attributes model
            
            color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            anchors.fill: parent
            anchors.leftMargin: 3

            font {
                family: XsStyle.fontFamily
            }

        }

        XsMenu {
            // x: control.x
            y: control.y - height
            ActionGroup {id: myActionGroup}
            hasCheckable: true
            
            id: popup
            title: ""
    
            Instantiator {
                model: combo_box_options
                XsMenuItem {
                    mytext: (combo_box_options && index >= 0) ? combo_box_options[index] : ""
                    mycheckable: true
                    actiongroup: myActionGroup
                    mychecked: mytext === control.value_
                    enabled: combo_box_options_enabled ? combo_box_options_enabled[index] : true
                    onTriggered: { value = mytext }
                }
                onObjectAdded: {
                    popup.insertItem(index, object)
                }
                onObjectRemoved: popup.removeItem(object)
            }
            function toggleShow() {
                if(visible) {
                    close()
                }
                else{
                    open()
                }
            }
        }
    
    }


}
