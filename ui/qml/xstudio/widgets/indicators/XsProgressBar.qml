// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

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