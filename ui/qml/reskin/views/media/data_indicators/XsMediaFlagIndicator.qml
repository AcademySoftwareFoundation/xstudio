// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudioReskin 1.0

Item{ 
    
    id: flagIndicator

    ListModel{ id: flagColorModel
        ListElement{
            flagColor: "#ff0000" //- "red"
        }
        ListElement{
            flagColor: "#28dc00" //- "green"
        }
        ListElement{
            flagColor: "#ffff00" //- "yellow"
        }
        ListElement{
            flagColor: "transparent"
        }
    }
   
    Rectangle{
        width: 4
        height: parent.height
        anchors.centerIn: parent
        color: index%3?flagColorModel.get(0).flagColor : index%2==0? flagColorModel.get(1).flagColor : "transparent"//flagColorModel.get(clrIndex).flagColor
        property int clrIndex: 0

    }

    Rectangle{
        width: headerThumbWidth; 
        height: parent.height
        anchors.right: parent.right
        color: bgColorPressed
    }

}