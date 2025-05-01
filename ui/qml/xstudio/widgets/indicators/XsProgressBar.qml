// SPDX-License-Identifier: Apache-2.0
import QtQuick



import xStudio 1.0

ProgressBar{

    // width: parent.width
    // height: 1

    indeterminate: false

    background: Rectangle{
        implicitWidth: 100
        implicitHeight: 1
        color: palette.highlight
    }

    // contentItem: Item{
    //     implicitWidth: 100
    //     implicitHeight: 1

    //     Rectangle{
    //         width: control.visualPosition * parent.width
    //         height: parent.height
    //         color: "red"
    //     }

    // }


}